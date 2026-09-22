// Text for the launcher window: a system font (found through fontconfig) rasterised with
// FreeType, each string rendered once into a texture and kept. Without a usable font the
// text falls back to SDL's built-in debug font.
#pragma once
#include <SDL3/SDL.h>
#include <map>
#include <string>

typedef struct FT_LibraryRec_* FT_Library;
typedef struct FT_FaceRec_* FT_Face;

namespace launcher
{

class Font
{
  public:
    // pattern is a fontconfig name such as "serif:bold"; size in pixels
    Font(SDL_Renderer* renderer, const char* pattern, int size);
    ~Font();
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;

    // width of the text in pixels
    int measure(const std::string& text);
    int getHeight() const
    {
        return m_size;
    }
    // draws the text with its top left corner at (x, y)
    void draw(const std::string& text, float x, float y, SDL_Color color);
    // the text shortened in the middle with an ellipsis until it fits width
    std::string fit(const std::string& text, int width);

  private:
    SDL_Texture* render(const std::string& text);

    SDL_Renderer* m_renderer;
    int m_size;
    FT_Library m_library;
    FT_Face m_face;
    std::map<std::string, SDL_Texture*> m_cache; // white text, tinted when drawn
};

} // namespace launcher
