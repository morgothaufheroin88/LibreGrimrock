// Bitmap fonts with a 256 pixel wide glyph atlas, reconstructed from Font.cpp
// (0x080db2d0-0x080dd480).
#pragma once
#include "core/Image.h"

namespace engine
{

class RenderableTexture;

class Font
{
  public:
    static constexpr int AtlasWidth = 256; // glyph atlas width in pixels
    static constexpr int NumGlyphs = 256;  // one glyph per byte value
    enum Builtin
    {
        Fixedsys = 0,
        Consolas12pt = 1
    };
    // 36 bytes in the original.
    struct Glyph
    {
        float u0, v0, u1, v1;
        int width, height;
        int xoffset, yoffset;
        int advance;
    };

    Font();
    // 0x080dc520: builtin fixed width fonts.
    Font(int builtin);
    // 0x080dc440: fixed width if charWidth > 0, otherwise a proportional font image
    // whose first row marks the glyph columns.
    Font(const char* filename, int charWidth, int spacing);
    virtual ~Font();

    // 0x080dc760: FreeType rasterised font, style 1 draws a black outline.
    static Font* loadTrueType(const char* filename, int size, int style);

    int getWidth(unsigned char c) const
    {
        return m_glyphs[c].width;
    }
    int getWidth(const char* text) const;
    bool isPrintable(unsigned char c) const
    {
        return m_glyphs[c].advance > 0;
    }
    const Glyph* getGlyph(unsigned char c) const
    {
        return &m_glyphs[c];
    }
    RenderableTexture* getTexture() const
    {
        return m_pTexture;
    }
    int getLineHeight() const
    {
        return m_lineHeight;
    }
    int getAscent() const
    {
        return m_ascent;
    }

  private:
    void initFixedWidth(const core::Image& image, const char* chars, int charWidth, int spacing);
    void initProportional(const core::Image& image, const char* chars, int spacing);

    Glyph m_glyphs[NumGlyphs];
    int m_lineHeight;
    int m_ascent;
    RenderableTexture* m_pTexture;
};

} // namespace engine
