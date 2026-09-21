// Reconstructed from Grimrock.bin.x86 Font.cpp.
#include "engine/Font.h"
#include "core/Array.h"
#include "core/Exception.h"
#include "core/FileSystem.h"
#include "engine/FontData.h"
#include "engine/Texture.h"
#include <cmath>
#include <cstring>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H
#include FT_STROKER_H

namespace engine
{

using namespace core;

namespace
{

// FreeType 26.6 fixed point: 64 units per pixel.
constexpr int FtUnitsShift = 6;
constexpr int FtUnitsPerPixel = 1 << FtUnitsShift;
constexpr int FtDpi = 72;

struct Span
{
    int x, y, width, coverage;
};

// 0x080db410
void rasterCallback(int y, int count, const FT_Span* spans, void* user)
{
    Array<Span>* out = (Array<Span>*)user;
    for (int i = 0; i < count; ++i)
    {
        Span span;
        span.x = spans[i].x;
        span.y = y;
        span.width = spans[i].len;
        span.coverage = spans[i].coverage;
        out->push_back(span);
    }
}

// 0x080db540
void renderSpans(FT_Library& library, FT_Outline* outline, Array<Span>* spans)
{
    FT_Raster_Params params;
    memset(&params, 0, sizeof(params));
    params.flags = FT_RASTER_FLAG_AA | FT_RASTER_FLAG_DIRECT;
    params.gray_spans = rasterCallback;
    params.user = spans;
    FT_Outline_Render(library, outline, &params);
}

// 0x080db5a0: glyph filled with fillColor over a strokeWidth wide outline.
Image* renderGlyphWithStroke(FT_Library& library, unsigned int glyphIndex, FT_Face& face,
                             const Color& fillColor, const Color& strokeColor, float strokeWidth,
                             int& xoffset, int& yoffset, int& advance)
{
    if (FT_Load_Glyph(face, glyphIndex, FT_LOAD_NO_BITMAP) != 0 ||
        face->glyph->format != FT_GLYPH_FORMAT_OUTLINE)
        return 0;
    FT_BBox box;
    FT_Outline_Get_CBox(&face->glyph->outline, &box);
    int border = (int)lrintf(ceilf(strokeWidth));
    // round the 26.6 fixed point box outwards to whole pixels and grow it by the border
    box.xMin = (box.xMin & ~(FtUnitsPerPixel - 1)) - border * FtUnitsPerPixel;
    box.yMin = (box.yMin & ~(FtUnitsPerPixel - 1)) - border * FtUnitsPerPixel;
    box.xMax =
        ((box.xMax + FtUnitsPerPixel - 1) & ~(FtUnitsPerPixel - 1)) + border * FtUnitsPerPixel;
    box.yMax =
        ((box.yMax + FtUnitsPerPixel - 1) & ~(FtUnitsPerPixel - 1)) + border * FtUnitsPerPixel;
    int top = (int)(box.yMax >> FtUnitsShift);
    FT_Outline_Translate(&face->glyph->outline, -box.xMin, -box.yMin);
    Array<Span> fill;
    renderSpans(library, &face->glyph->outline, &fill);
    FT_Outline_Translate(&face->glyph->outline, box.xMin, box.yMin);

    Array<Span> outline;
    FT_Stroker stroker;
    FT_Stroker_New(library, &stroker);
    FT_Stroker_Set(stroker, (int)lrintf(strokeWidth * FtUnitsPerPixel), FT_STROKER_LINECAP_ROUND,
                   FT_STROKER_LINEJOIN_ROUND, 0);
    FT_Glyph glyph;
    if (FT_Get_Glyph(face->glyph, &glyph) != 0)
    {
        FT_Stroker_Done(stroker);
        return 0;
    }
    FT_Glyph_StrokeBorder(&glyph, stroker, 0, 1);
    if (glyph->format == FT_GLYPH_FORMAT_OUTLINE)
    {
        FT_Outline* o = &((FT_OutlineGlyph)glyph)->outline;
        FT_Outline_Translate(o, -box.xMin, -box.yMin);
        renderSpans(library, o, &outline);
        FT_Outline_Translate(o, box.xMin, box.yMin);
    }
    FT_Stroker_Done(stroker);
    FT_Done_Glyph(glyph);
    if (fill.size() <= 0)
        return 0;

    xoffset = (int)(box.xMin >> FtUnitsShift);
    yoffset = top;
    advance = (int)(face->glyph->advance.x >> FtUnitsShift);
    int height = (int)((box.yMax - box.yMin) >> FtUnitsShift);
    Image* image = new Image((int)((box.xMax - box.xMin) >> FtUnitsShift), height);
    for (int i = 0; i < outline.size(); ++i)
    {
        const Span& span = outline[i];
        for (int x = 0; x < span.width; ++x)
            image->setPixelSafe(span.x + x, height - 1 - span.y,
                                Color(strokeColor.r, strokeColor.g, strokeColor.b, span.coverage));
    }
    for (int i = 0; i < fill.size(); ++i)
    {
        const Span& span = fill[i];
        for (int x = 0; x < span.width; ++x)
        {
            int px = span.x + x, py = height - 1 - span.y;
            Color pixel = image->getPixelSafe(px, py);
            int coverage = span.coverage;
            pixel.r = (unsigned char)lrintf(pixel.r +
                                            (float)((fillColor.r - pixel.r) * coverage) / 255.0f);
            pixel.g = (unsigned char)lrintf((float)((fillColor.g - pixel.g) * coverage) / 255.0f +
                                            pixel.g);
            pixel.b = (unsigned char)lrintf(pixel.b +
                                            (float)((fillColor.b - pixel.b) * coverage) / 255.0f);
            pixel.a = (unsigned char)(coverage + pixel.a < 256 ? coverage + pixel.a : 255);
            image->setPixelSafe(px, py, pixel);
        }
    }
    return image;
}

} // namespace

// 0x080db2d0
Font::Font() : m_lineHeight(0), m_ascent(0), m_pTexture(0)
{
    memset(m_glyphs, 0, sizeof(m_glyphs));
}

// 0x080dc520
Font::Font(int builtin) : m_lineHeight(0), m_ascent(0), m_pTexture(0)
{
    memset(m_glyphs, 0, sizeof(m_glyphs));
    struct BuiltinFont
    {
        const unsigned char* data;
        int format; // 0 = 1 bit per pixel, 1 = 8 bit alpha
        int width, height, charWidth;
    };
    static const BuiltinFont fonts[2] = {{g_fixedsys, 0, 776, 15, 8},
                                         {g_consolas_12pt, 1, 679, 13, 7}};
    const BuiltinFont& font = fonts[builtin];
    Image image(font.width, font.height);
    const unsigned char* data = font.data;
    if (font.format == 0)
    {
        int bit = 0;
        for (int y = 0; y < image.getHeight(); ++y)
        {
            for (int x = 0; x < image.getWidth(); ++x)
            {
                unsigned char alpha = ((*data >> bit) & 1) ? 255 : 0;
                if (++bit == 8)
                {
                    bit = 0;
                    ++data;
                }
                image.setPixel(x, y, Color(255, 255, 255, alpha));
            }
        }
    }
    else
    {
        for (int y = 0; y < image.getHeight(); ++y)
            for (int x = 0; x < image.getWidth(); ++x)
                image.setPixel(x, y, Color(255, 255, 255, *data++));
    }
    initFixedWidth(image, g_glyphs, font.charWidth, 0);
}

// 0x080dc440
Font::Font(const char* filename, int charWidth, int spacing)
    : m_lineHeight(0), m_ascent(0), m_pTexture(0)
{
    memset(m_glyphs, 0, sizeof(m_glyphs));
    Image image(filename);
    if (charWidth > 0)
        initFixedWidth(image, g_glyphs, charWidth, spacing);
    else
        initProportional(image, g_glyphs, spacing);
}

// 0x080dd450
Font::~Font()
{
    delete m_pTexture;
}

// 0x080dc190
int Font::getWidth(const char* text) const
{
    int width = 0;
    size_t len = strlen(text);
    for (size_t i = 0; i < len; ++i)
        width += m_glyphs[(unsigned char)text[i]].advance;
    return width;
}

static int nextPow2(int v)
{
    int pow2 = 1;
    while (pow2 < v)
        pow2 *= 2;
    return pow2;
}

// 0x080dbb00: glyph strip image, charWidth pixels per character, packed into an AtlasWidth
// wide atlas.
void Font::initFixedWidth(const Image& image, const char* chars, int charWidth, int spacing)
{
    int numChars = (int)strlen(chars);
    if (image.getWidth() != numChars * charWidth)
        throw Exception("Image size is invalid for a fixed width font");
    int charHeight = image.getHeight();
    int atlasHeight = nextPow2((numChars / (Font::AtlasWidth / charWidth) + 1) * charHeight);
    m_lineHeight = charHeight;
    Image atlas(Font::AtlasWidth, atlasHeight);
    memset(m_glyphs, 0, sizeof(m_glyphs));
    int x = 0, y = 0;
    for (int i = 0; i < numChars; ++i)
    {
        if (x + charWidth > Font::AtlasWidth)
        {
            x = 0;
            y += charHeight;
        }
        atlas.copy(x, y, &image, i * charWidth, 0, charWidth, charHeight);
        Glyph& g = m_glyphs[(unsigned char)chars[i]];
        g.width = charWidth;
        g.height = charHeight;
        g.u0 = x / (float)Font::AtlasWidth;
        g.v0 = (float)y / atlasHeight;
        g.u1 = g.u0 + charWidth / (float)Font::AtlasWidth;
        g.v1 = g.v0 + (float)charHeight / atlasHeight;
        g.advance = charWidth + spacing;
        x += charWidth;
    }
    m_pTexture = createRenderableTexture(atlas);
    m_glyphs[' '].advance = charWidth;
}

// 0x080dbd80: the first image row marks glyph columns with non zero pixels.
void Font::initProportional(const Image& image, const char* chars, int spacing)
{
    struct Cell
    {
        int srcX, width, x, y;
    };
    int numChars = (int)strlen(chars);
    int imageWidth = image.getWidth();
    const unsigned int* row0 = (const unsigned int*)image.getData();
    m_lineHeight = image.getHeight() - 1;
    Array<Cell> cells;
    int x = 0, y = 0, srcX = 0;
    while (cells.size() < numChars)
    {
        // find the next marked column run
        while (srcX < imageWidth && row0[srcX] == 0)
            ++srcX;
        int start = srcX;
        int width = 0;
        if (srcX < imageWidth)
        {
            while (srcX < imageWidth && row0[srcX] != 0)
                ++srcX;
            width = srcX - start;
        }
        if (x + width > 255)
        {
            x = 0;
            y += m_lineHeight + 1;
        }
        Cell c = {start, width, x, y};
        cells.push_back(c);
        x += width + 1;
        if (srcX >= imageWidth && width == 0)
            break;
    }
    int atlasHeight = nextPow2(y + m_lineHeight + 1);
    Image atlas(Font::AtlasWidth, atlasHeight);
    for (int i = 0; i < cells.size(); ++i)
        atlas.copy(cells[i].x, cells[i].y, &image, cells[i].srcX, 1, cells[i].width, m_lineHeight);
    m_pTexture = createRenderableTexture(atlas);
    memset(m_glyphs, 0, sizeof(m_glyphs));
    for (int i = 0; i < cells.size() && i < numChars; ++i)
    {
        Glyph& g = m_glyphs[(unsigned char)chars[i]];
        g.width = cells[i].width;
        g.height = m_lineHeight;
        g.u0 = cells[i].x / (float)Font::AtlasWidth;
        g.v0 = (float)cells[i].y / atlasHeight;
        g.u1 = (cells[i].x + cells[i].width) / (float)Font::AtlasWidth;
        g.v1 = (float)(cells[i].y + m_lineHeight) / atlasHeight;
        g.advance = spacing + g.width;
    }
    m_glyphs[' '].advance =
        m_glyphs['_'].advance > 0 ? m_glyphs['_'].advance : m_glyphs['o'].advance;
}

// 0x080dc760
Font* Font::loadTrueType(const char* filename, int size, int style)
{
    FT_Library library;
    int err = FT_Init_FreeType(&library);
    if (err)
        throw Exception("FT_Init_FreeType failed with error code %d", err);
    int fileSize;
    char* fileData = readFile(filename, fileSize);
    FT_Face face;
    err = FT_New_Memory_Face(library, (const FT_Byte*)fileData, fileSize, 0, &face);
    if (err)
        throw Exception("FT_New_Face failed with error code %d", err);
    err = FT_Set_Char_Size(face, size << FtUnitsShift, 0, FtDpi, 0);
    if (err)
        throw Exception("FT_Set_Char_Size failed with error code %d", err);

    Font* font = new Font;
    Image* images[Font::NumGlyphs];
    memset(images, 0, sizeof(images));
    for (int c = 0; c < Font::NumGlyphs; ++c)
    {
        char str[2] = {(char)c, 0};
        if (c != ' ' && !strstr(g_glyphs, str))
            continue;
        unsigned int index = FT_Get_Char_Index(face, c);
        Glyph& g = font->m_glyphs[c];
        if (c == ' ' || style != 1)
        {
            if (FT_Load_Glyph(face, index, FT_LOAD_RENDER) != 0)
                continue;
            FT_GlyphSlot slot = face->glyph;
            Image* image = new Image(slot->bitmap.width, slot->bitmap.rows);
            for (int y = 0; y < (int)slot->bitmap.rows; ++y)
                for (int x = 0; x < (int)slot->bitmap.width; ++x)
                    image->setPixel(
                        x, y,
                        Color(255, 255, 255, slot->bitmap.buffer[y * slot->bitmap.width + x]));
            images[c] = image;
            g.width = slot->bitmap.width;
            g.height = slot->bitmap.rows;
            g.xoffset = slot->bitmap_left;
            g.yoffset = -slot->bitmap_top;
            g.advance = (int)(slot->advance.x >> FtUnitsShift);
        }
        else
        {
            int xoffset, yoffset, advance;
            Image* image = renderGlyphWithStroke(library, index, face, Color::White, Color::Black,
                                                 1.0f, xoffset, yoffset, advance);
            images[c] = image;
            if (image)
            {
                g.width = image->getWidth();
                g.height = image->getHeight();
                g.xoffset = xoffset;
                g.yoffset = -yoffset;
                g.advance = advance;
            }
        }
    }
    // pack the glyphs into rows of an AtlasWidth pixel wide atlas
    int xs[Font::NumGlyphs], ys[Font::NumGlyphs];
    int x = 0, y = 0, rowHeight = 0;
    for (int c = 0; c < Font::NumGlyphs; ++c)
    {
        xs[c] = ys[c] = 0;
        const Glyph& g = font->m_glyphs[c];
        if (g.width <= 0)
            continue;
        if (x + g.width > Font::AtlasWidth)
        {
            y += rowHeight;
            x = 0;
            rowHeight = 0;
        }
        xs[c] = x;
        ys[c] = y;
        x += g.width;
        if (g.height > rowHeight)
            rowHeight = g.height;
    }
    int atlasHeight = nextPow2(y + rowHeight);
    Image atlas(Font::AtlasWidth, atlasHeight);
    for (int c = 0; c < Font::NumGlyphs; ++c)
        if (font->m_glyphs[c].width > 0 && images[c])
            atlas.copy(xs[c], ys[c], images[c], 0, 0, images[c]->getWidth(),
                       images[c]->getHeight());
    font->m_pTexture = createRenderableTexture(atlas);
    for (int c = 0; c < Font::NumGlyphs; ++c)
    {
        Glyph& g = font->m_glyphs[c];
        if (g.width <= 0)
            continue;
        g.u0 = xs[c] / (float)Font::AtlasWidth;
        g.v0 = (float)ys[c] / atlasHeight;
        g.u1 = (xs[c] + g.width) / (float)Font::AtlasWidth;
        g.v1 = (float)(ys[c] + g.height) / atlasHeight;
    }
    font->m_lineHeight = (int)(face->size->metrics.height >> FtUnitsShift);
    int ascent = 0;
    for (int c = 0; c < Font::NumGlyphs; ++c)
        if (-font->m_glyphs[c].yoffset > ascent)
            ascent = -font->m_glyphs[c].yoffset;
    font->m_ascent = ascent;
    FT_Done_Face(face);
    FT_Done_FreeType(library);
    delete[] fileData;
    for (int c = 0; c < Font::NumGlyphs; ++c)
        delete images[c];
    return font;
}

} // namespace engine
