#include "Font.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <vector>
#if LAUNCHER_HAVE_FONTCONFIG
#include <fontconfig/fontconfig.h>
#endif

namespace launcher
{

// the file of the best match for a fontconfig pattern, or a few common fonts
static std::string findFontFile(const char* pattern)
{
#if LAUNCHER_HAVE_FONTCONFIG
    std::string file;
    if (FcInit())
    {
        FcPattern* request = FcNameParse((const FcChar8*)pattern);
        FcConfigSubstitute(nullptr, request, FcMatchPattern);
        FcDefaultSubstitute(request);
        FcResult result;
        if (FcPattern* match = FcFontMatch(nullptr, request, &result))
        {
            FcChar8* path = nullptr;
            if (FcPatternGetString(match, FC_FILE, 0, &path) == FcResultMatch)
                file = (const char*)path;
            FcPatternDestroy(match);
        }
        FcPatternDestroy(request);
    }
    if (!file.empty())
        return file;
#endif
    const char* fallbacks[] = {"/usr/share/fonts/dejavu/DejaVuSans.ttf",
                               "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                               "/usr/share/fonts/liberation-fonts/LiberationSans-Regular.ttf"};
    for (const char* path : fallbacks)
        if (SDL_GetPathInfo(path, nullptr))
            return path;
    return "";
}

// the code points of a UTF-8 string (paths may hold any script)
static std::vector<Uint32> decodeUtf8(const std::string& text)
{
    std::vector<Uint32> codepoints;
    const char* p = text.c_str();
    size_t left = text.size();
    while (left > 0)
    {
        Uint32 codepoint = SDL_StepUTF8(&p, &left);
        if (codepoint == 0)
            break;
        codepoints.push_back(codepoint);
    }
    return codepoints;
}

Font::Font(SDL_Renderer* renderer, const char* pattern, int size)
    : m_renderer(renderer), m_size(size), m_library(nullptr), m_face(nullptr)
{
    std::string file = findFontFile(pattern);
    if (file.empty() || FT_Init_FreeType(&m_library) != 0)
        return;
    if (FT_New_Face(m_library, file.c_str(), 0, &m_face) != 0)
    {
        m_face = nullptr;
        return;
    }
    FT_Set_Pixel_Sizes(m_face, 0, size);
}

Font::~Font()
{
    for (auto& entry : m_cache)
        SDL_DestroyTexture(entry.second);
    if (m_face)
        FT_Done_Face(m_face);
    if (m_library)
        FT_Done_FreeType(m_library);
}

int Font::measure(const std::string& text)
{
    if (!m_face)
        return (int)decodeUtf8(text).size() * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * m_size / 12;
    int width = 0;
    for (Uint32 codepoint : decodeUtf8(text))
        if (FT_Load_Char(m_face, codepoint, FT_LOAD_DEFAULT) == 0)
            width += (int)(m_face->glyph->advance.x >> 6);
    return width;
}

SDL_Texture* Font::render(const std::string& text)
{
    auto cached = m_cache.find(text);
    if (cached != m_cache.end())
        return cached->second;
    std::vector<Uint32> codepoints = decodeUtf8(text);
    int ascent = (int)(m_face->size->metrics.ascender >> 6);
    int height = (int)((m_face->size->metrics.ascender - m_face->size->metrics.descender) >> 6);
    // the width is the sum of the advances; a glyph may overhang it a little
    int width = measure(text);
    SDL_Texture* texture = nullptr;
    if (width > 0 && height > 0)
    {
        SDL_Surface* surface = SDL_CreateSurface(width + 2, height, SDL_PIXELFORMAT_RGBA32);
        SDL_FillSurfaceRect(surface, nullptr, 0);
        Uint8* pixels = (Uint8*)surface->pixels;
        int penX = 0;
        for (Uint32 codepoint : codepoints)
        {
            if (FT_Load_Char(m_face, codepoint, FT_LOAD_RENDER) != 0)
                continue;
            FT_GlyphSlot glyph = m_face->glyph;
            const FT_Bitmap& bitmap = glyph->bitmap;
            for (unsigned row = 0; row < bitmap.rows; ++row)
            {
                int y = ascent - glyph->bitmap_top + (int)row;
                if (y < 0 || y >= height)
                    continue;
                for (unsigned column = 0; column < bitmap.width; ++column)
                {
                    int x = penX + glyph->bitmap_left + (int)column;
                    if (x < 0 || x >= surface->w)
                        continue;
                    Uint8 coverage = bitmap.buffer[row * bitmap.pitch + column];
                    Uint8* pixel = pixels + y * surface->pitch + x * 4;
                    pixel[0] = pixel[1] = pixel[2] = 255;
                    if (coverage > pixel[3])
                        pixel[3] = coverage;
                }
            }
            penX += (int)(glyph->advance.x >> 6);
        }
        texture = SDL_CreateTextureFromSurface(m_renderer, surface);
        SDL_DestroySurface(surface);
    }
    m_cache[text] = texture;
    return texture;
}

void Font::draw(const std::string& text, float x, float y, SDL_Color color)
{
    if (!m_face)
    {
        float scale = (float)m_size / 12.0f;
        SDL_SetRenderScale(m_renderer, scale, scale);
        SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
        SDL_RenderDebugText(m_renderer, x / scale, y / scale, text.c_str());
        SDL_SetRenderScale(m_renderer, 1.0f, 1.0f);
        return;
    }
    SDL_Texture* texture = render(text);
    if (!texture)
        return;
    float width = 0, height = 0;
    SDL_GetTextureSize(texture, &width, &height);
    SDL_SetTextureColorMod(texture, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(texture, color.a);
    SDL_FRect target = {x, y, width, height};
    SDL_RenderTexture(m_renderer, texture, nullptr, &target);
}

std::string Font::fit(const std::string& text, int width)
{
    if (measure(text) <= width)
        return text;
    std::vector<Uint32> codepoints = decodeUtf8(text);
    // drop characters from the middle until "start…end" fits
    for (size_t keep = codepoints.size(); keep > 4; --keep)
    {
        size_t head = keep / 3, tail = keep - head;
        std::string shortened;
        for (size_t i = 0; i < head; ++i)
        {
            char buffer[5] = {0};
            SDL_UCS4ToUTF8(codepoints[i], buffer);
            shortened += buffer;
        }
        shortened += "\xE2\x80\xA6"; // …
        for (size_t i = codepoints.size() - tail; i < codepoints.size(); ++i)
        {
            char buffer[5] = {0};
            SDL_UCS4ToUTF8(codepoints[i], buffer);
            shortened += buffer;
        }
        if (measure(shortened) <= width)
            return shortened;
    }
    return "\xE2\x80\xA6";
}

} // namespace launcher
