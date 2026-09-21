// Reconstructed from Grimrock.bin.x86 Image.cpp.
#include "core/Image.h"
#include "core/Exception.h"
#include "core/FileSystem.h"
#include "core/Sys.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <cstring>

namespace core
{

// 0x080ca680
Image::Image() : m_width(0), m_height(0), m_pData(0) {}
// 0x080cb680
Image::Image(int width, int height) : m_width(width), m_height(height)
{
    static bool imageLibInitialized = false;
    if (!imageLibInitialized)
    {
        IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_TIF);
        imageLibInitialized = true;
    }
    m_pData = new unsigned char[(size_t)width * height * 4];
    memset(m_pData, 0, (size_t)width * height * 4);
}
// 0x080cb710
Image::Image(const Image& other) : m_width(other.m_width), m_height(other.m_height)
{
    m_pData = new unsigned char[(size_t)m_width * m_height * 4];
    memcpy(m_pData, other.m_pData, (size_t)m_width * m_height * 4);
}
// 0x080caf30
Image::~Image()
{
    if (m_pData)
        delete[] m_pData;
}
// 0x080cb150
Image& Image::operator=(const Image& other)
{
    if (this != &other)
    {
        if (m_pData)
            delete[] m_pData;
        m_width = other.m_width;
        m_height = other.m_height;
        m_pData = new unsigned char[(size_t)m_width * m_height * 4];
        memcpy(m_pData, other.m_pData, (size_t)m_width * m_height * 4);
    }
    return *this;
}

// 0x080cb1d0: reads through the mounted file systems.
Image::Image(const char* filename) : m_width(0), m_height(0), m_pData(0)
{
    int length = 0;
    char* bytes;
    try
    {
        bytes = readFile(filename, length);
    }
    catch (Exception&)
    {
        throw Exception("Error loading: %s", filename);
    }
    // The original went through FreeImage: the type from the file signature, then from
    // the file name (TGA has no signature). SDL_image needs the same two steps.
    String ext = getFileExtension(filename);
    ext.toupper();
    SDL_RWops* rw = SDL_RWFromConstMem(bytes, length);
    SDL_Surface* loaded = 0;
    if (IMG_isPNG(rw) || IMG_isJPG(rw) || IMG_isBMP(rw) || IMG_isTIF(rw) || IMG_isGIF(rw))
        loaded = IMG_Load_RW(rw, 0);
    if (!loaded)
    {
        SDL_RWseek(rw, 0, RW_SEEK_SET);
        loaded = IMG_LoadTyped_RW(rw, 0, ext.c_str());
    }
    SDL_RWclose(rw);
    delete[] bytes;
    if (!loaded)
    {
        debugPrint("IMG_Load(%s): %s\n", filename, IMG_GetError());
        throw Exception("Error loading: %s", filename);
    }
    SDL_Surface* surface = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(loaded);
    if (!surface)
        throw Exception("Error loading: %s", filename);
    m_width = surface->w;
    m_height = surface->h;
    m_pData = new unsigned char[(size_t)m_width * m_height * 4];
    for (int y = 0; y < m_height; ++y)
        memcpy(m_pData + (size_t)y * m_width * 4, (const char*)surface->pixels + y * surface->pitch,
               (size_t)m_width * 4);
    SDL_FreeSurface(surface);
}

// 0x080caf90
void Image::save(const char* filename, bool alpha)
{
    String ext = getFileExtension(filename);
    ext.tolower();
    int bpp = alpha ? 32 : 24;
    SDL_Surface* surface =
        SDL_CreateRGBSurface(0, m_width, m_height, bpp, PixelMaskRed, PixelMaskGreen, PixelMaskBlue,
                             alpha ? PixelMaskAlpha : 0);
    if (!surface)
        throw Exception("Unknown file format: %s", filename);
    int bytes = bpp / 8;
    for (int y = 0; y < m_height; ++y)
    {
        unsigned char* dst = (unsigned char*)surface->pixels + y * surface->pitch;
        const unsigned char* src = m_pData + (size_t)y * m_width * 4;
        for (int x = 0; x < m_width; ++x)
            memcpy(dst + x * bytes, src + x * 4, bytes);
    }
    int result;
    if (ext == "png")
        result = IMG_SavePNG(surface, filename);
    else if (ext == "jpg" || ext == "jpeg")
        result = IMG_SaveJPG(surface, filename, 90);
    else if (ext == "bmp")
        result = SDL_SaveBMP(surface, filename);
    else
    {
        SDL_FreeSurface(surface);
        throw Exception("Unknown file format: %s", filename);
    }
    SDL_FreeSurface(surface);
    if (result != 0)
        throw Exception("Error saving: %s", filename);
}

// 0x080ca6a0
void Image::clear(const Color& color)
{
    for (int y = 0; y < m_height; ++y)
        for (int x = 0; x < m_width; ++x)
            setPixel(x, y, color);
}
// 0x080ca6f0
void Image::setPixel(int x, int y, const Color& c)
{
    unsigned char* p = m_pData + ((size_t)y * m_width + x) * 4;
    p[0] = c.b;
    p[1] = c.g;
    p[2] = c.r;
    p[3] = c.a;
}
// 0x080ca730
Color Image::getPixel(int x, int y) const
{
    const unsigned char* p = m_pData + ((size_t)y * m_width + x) * 4;
    return Color((int)p[2], (int)p[1], (int)p[0], (int)p[3]);
}
// 0x080ca780
void Image::setPixelSafe(int x, int y, const Color& c)
{
    if (x >= 0 && y >= 0 && x < m_width && y < m_height)
        setPixel(x, y, c);
}
// 0x080ca7f0
Color Image::getPixelSafe(int x, int y) const
{
    if (x >= 0 && y >= 0 && x < m_width && y < m_height)
        return getPixel(x, y);
    return Color(0, 0, 0, 0);
}
// 0x080ca870
void Image::flipY()
{
    size_t rowBytes = (size_t)m_width * 4;
    unsigned char* rowBuffer = new unsigned char[rowBytes];
    for (int y = 0; y < m_height / 2; ++y)
    {
        unsigned char* topRow = m_pData + y * rowBytes;
        unsigned char* bottomRow = m_pData + (m_height - 1 - y) * rowBytes;
        memcpy(rowBuffer, topRow, rowBytes);
        memcpy(topRow, bottomRow, rowBytes);
        memcpy(bottomRow, rowBuffer, rowBytes);
    }
    delete[] rowBuffer;
}
// 0x080ca8f0: clipped rectangle copy.
void Image::copy(int x, int y, const Image* src, int srcX, int srcY, int width, int height)
{
    for (int j = 0; j < height; ++j)
    {
        for (int i = 0; i < width; ++i)
        {
            int dx = x + i, dy = y + j;
            if (dx >= 0 && dy >= 0 && dx < m_width && dy < m_height)
                memcpy(m_pData + ((size_t)dy * m_width + dx) * 4,
                       src->m_pData + ((size_t)(srcY + j) * src->m_width + srcX + i) * 4, 4);
        }
    }
}
// 0x080ca9d0: 2x2 box filter.
void Image::halvePow2()
{
    int w = m_width / 2, h = m_height / 2;
    unsigned char* out = new unsigned char[(size_t)w * h * 4];
    for (int y = 0; y < h; ++y)
    {
        const unsigned char* row0 = m_pData + (size_t)(y * 2) * m_width * 4;
        const unsigned char* row1 = row0 + (size_t)m_width * 4;
        unsigned char* dst = out + (size_t)y * w * 4;
        for (int x = 0; x < w; ++x)
        {
            for (int c = 0; c < 4; ++c)
                dst[x * 4 + c] = (unsigned char)((row0[x * 8 + c] + row0[x * 8 + 4 + c] +
                                                  row1[x * 8 + c] + row1[x * 8 + 4 + c]) >>
                                                 2);
        }
    }
    m_width = w;
    m_height = h;
    delete[] m_pData;
    m_pData = out;
}
// 0x080cab60: 3x3 tent filter (1/16 2/16 1/16 ...) with clamped sampling for odd sizes.
void Image::halve()
{
    if (m_width > 0 && (m_width & (m_width - 1)) == 0 && m_height > 0 &&
        (m_height & (m_height - 1)) == 0)
    {
        halvePow2();
        return;
    }
    int maxX = m_width - 1, maxY = m_height - 1;
    int halfWidth = m_width / 2 > 0 ? m_width / 2 : 1;
    int halfHeight = m_height / 2 > 0 ? m_height / 2 : 1;
    unsigned char* out = new unsigned char[(size_t)halfWidth * halfHeight * 4];
    for (int y = 0; y < halfHeight; ++y)
    {
        int y0 = y * 2 < maxY ? y * 2 : maxY;
        int y1 = y * 2 + 1 < maxY ? y * 2 + 1 : maxY;
        int y2 = y * 2 + 2 < maxY ? y * 2 + 2 : maxY;
        for (int x = 0; x < halfWidth; ++x)
        {
            int x0 = x * 2 <= maxX ? x * 2 : maxX;
            int x1 = x * 2 + 1 <= maxX ? x * 2 + 1 : maxX;
            int x2 = x * 2 + 2 <= maxX ? x * 2 + 2 : maxX;
            for (int c = 0; c < 4; ++c)
            {
                const unsigned char* channel = m_pData + c;
#define PX(xx, yy) (unsigned)channel[((size_t)(yy) * m_width + (xx)) * 4]
                unsigned v = (PX(x0, y0) >> 4) + (PX(x1, y0) >> 3) + (PX(x2, y0) >> 4) +
                             (PX(x0, y1) >> 3) + (PX(x1, y1) >> 2) + (PX(x2, y1) >> 3) +
                             (PX(x0, y2) >> 4) + (PX(x1, y2) >> 3) + (PX(x2, y2) >> 4);
#undef PX
                out[((size_t)y * halfWidth + x) * 4 + c] = (unsigned char)(v > 255 ? 255 : v);
            }
        }
    }
    m_width = halfWidth;
    m_height = halfHeight;
    delete[] m_pData;
    m_pData = out;
}

} // namespace core
