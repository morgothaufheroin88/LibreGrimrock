// Reconstructed from Grimrock.bin.x86 Image.cpp.
#include "core/Image.h"
#include "core/Exception.h"
#include "core/FileSystem.h"
#include "core/Sys.h"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_TGA
#define STBI_ONLY_GIF
#include <cmath>
#include <cstring>
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

namespace core
{

// 0x080ca680
Image::Image() : m_width(0), m_height(0), m_pData(0) {}
// 0x080cb680
Image::Image(int width, int height) : m_width(width), m_height(height)
{
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
    // The original went through FreeImage (PNG, JPEG, BMP, TGA, GIF are what the game
    // ships); stb_image decodes the same formats to RGBA, converted here to the BGRA
    // layout of Image.
    int width = 0, height = 0, components = 0;
    unsigned char* rgba =
        stbi_load_from_memory((const unsigned char*)bytes, length, &width, &height, &components, 4);
    delete[] bytes;
    if (!rgba)
    {
        debugPrint("Image::load(%s): %s\n", filename, stbi_failure_reason());
        throw Exception("Error loading: %s", filename);
    }
    m_width = width;
    m_height = height;
    m_pData = new unsigned char[(size_t)m_width * m_height * 4];
    for (size_t i = 0; i < (size_t)m_width * m_height; ++i)
    {
        m_pData[i * 4 + 0] = rgba[i * 4 + 2];
        m_pData[i * 4 + 1] = rgba[i * 4 + 1];
        m_pData[i * 4 + 2] = rgba[i * 4 + 0];
        m_pData[i * 4 + 3] = rgba[i * 4 + 3];
    }
    stbi_image_free(rgba);
}

// 0x080caf90
void Image::save(const char* filename, bool alpha)
{
    String ext = getFileExtension(filename);
    ext.tolower();
    if (ext != "png" && ext != "jpg" && ext != "jpeg" && ext != "bmp")
        throw Exception("Unknown file format: %s", filename);
    int components = alpha ? 4 : 3;
    unsigned char* pixels = new unsigned char[(size_t)m_width * m_height * components];
    for (size_t i = 0; i < (size_t)m_width * m_height; ++i)
    {
        pixels[i * components + 0] = m_pData[i * 4 + 2];
        pixels[i * components + 1] = m_pData[i * 4 + 1];
        pixels[i * components + 2] = m_pData[i * 4 + 0];
        if (alpha)
            pixels[i * components + 3] = m_pData[i * 4 + 3];
    }
    int result;
    if (ext == "png")
        result =
            stbi_write_png(filename, m_width, m_height, components, pixels, m_width * components);
    else if (ext == "bmp")
        result = stbi_write_bmp(filename, m_width, m_height, components, pixels);
    else
        result = stbi_write_jpg(filename, m_width, m_height, components, pixels, 90);
    delete[] pixels;
    if (result == 0)
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

#if GRIMROCK_GAME >= 2
// 0x004997f0: the rectangle is clipped to the image
void Image::fillRect(int x, int y, int width, int height, const Color& color)
{
    int x0 = x < 0 ? 0 : (x > m_width - 1 ? m_width - 1 : x);
    int y0 = y < 0 ? 0 : (y > m_height - 1 ? m_height - 1 : y);
    int x1 = x + width - 1, y1 = y + height - 1;
    x1 = x1 < 0 ? 0 : (x1 > m_width - 1 ? m_width - 1 : x1);
    y1 = y1 < 0 ? 0 : (y1 > m_height - 1 ? m_height - 1 : y1);
    for (int py = y0; py <= y1; ++py)
        for (int px = x0; px <= x1; ++px)
            setPixel(px, py, color);
}
// 0x00499ac0
Vec4 Image::sampleNearestClamp(float x, float y) const
{
    int ix = (int)(floorf(x + 0.5f));
    int iy = (int)(floorf(y + 0.5f));
    ix = ix < 0 ? 0 : (ix > m_width - 1 ? m_width - 1 : ix);
    iy = iy < 0 ? 0 : (iy > m_height - 1 ? m_height - 1 : iy);
    Color c = getPixel(ix, iy);
    return Vec4((float)c.r, (float)c.g, (float)c.b, (float)c.a);
}
// 0x00499ba0
Vec4 Image::sampleLinearClamp(float x, float y) const
{
    x = x < 0.0f ? 0.0f : (x > (float)(m_width - 1) ? (float)(m_width - 1) : x);
    y = y < 0.0f ? 0.0f : (y > (float)(m_height - 1) ? (float)(m_height - 1) : y);
    int x0 = (int)floorf(x), y0 = (int)floorf(y);
    float fx = x - (float)x0, fy = y - (float)y0;
    int x1 = x0 + 1 < m_width - 1 ? x0 + 1 : m_width - 1;
    int y1 = y0 + 1 < m_height - 1 ? y0 + 1 : m_height - 1;
    Color c00 = getPixel(x0, y0), c10 = getPixel(x1, y0), c01 = getPixel(x0, y1),
          c11 = getPixel(x1, y1);
    float w00 = (1.0f - fx) * (1.0f - fy), w10 = fx * (1.0f - fy), w01 = (1.0f - fx) * fy,
          w11 = fx * fy;
    return Vec4(w00 * c00.r + w10 * c10.r + w01 * c01.r + w11 * c11.r,
                w00 * c00.g + w10 * c10.g + w01 * c01.g + w11 * c11.g,
                w00 * c00.b + w10 * c10.b + w01 * c01.b + w11 * c11.b,
                w00 * c00.a + w10 * c10.a + w01 * c01.a + w11 * c11.a);
}
// 0x00499e60: samples at the pixel centres of the new grid
void Image::resample(int width, int height)
{
    unsigned char* data = new unsigned char[(size_t)width * height * 4];
    float scaleX = (float)width / (float)m_width;
    float scaleY = (float)height / (float)m_height;
    float offsetX = (scaleX - 1.0f) * 0.5f;
    float offsetY = (scaleY - 1.0f) * 0.5f;
    unsigned char* out = data;
    for (int y = 0; y < height; ++y)
    {
        float sy = ((float)y - offsetY) / scaleY;
        for (int x = 0; x < width; ++x)
        {
            float sx = ((float)x - offsetX) / scaleX;
            Vec4 c = sampleLinearClamp(sx, sy);
            out[0] = (unsigned char)(int)(c.z + 0.5f);
            out[1] = (unsigned char)(int)(c.y + 0.5f);
            out[2] = (unsigned char)(int)(c.x + 0.5f);
            out[3] = (unsigned char)(int)(c.w + 0.5f);
            out += 4;
        }
    }
    delete[] m_pData;
    m_pData = data;
    m_width = width;
    m_height = height;
}
// 0x00499ff0
void Image::blur(int kernel)
{
    static constexpr float gaussian[9] = {0.0625f, 0.125f,  0.0625f, 0.125f, 0.25f,
                                          0.125f,  0.0625f, 0.125f,  0.0625f};
    static constexpr float gentle[9] = {0.01f, 0.08f, 0.01f, 0.08f, 0.64f,
                                        0.08f, 0.01f, 0.08f, 0.01f};
    const float* weights = kernel == 1 ? gaussian : gentle;
    unsigned char* data = new unsigned char[(size_t)m_width * m_height * 4];
    unsigned char* out = data;
    for (int y = 0; y < m_height; ++y)
    {
        for (int x = 0; x < m_width; ++x)
        {
            float sum[4] = {0, 0, 0, 0};
            for (int ky = -1; ky <= 1; ++ky)
            {
                int sy = y + ky;
                sy = sy < 0 ? 0 : (sy > m_height - 1 ? m_height - 1 : sy);
                for (int kx = -1; kx <= 1; ++kx)
                {
                    int sx = x + kx;
                    sx = sx < 0 ? 0 : (sx > m_width - 1 ? m_width - 1 : sx);
                    const unsigned char* p = m_pData + ((size_t)sy * m_width + sx) * 4;
                    float w = weights[(ky + 1) * 3 + kx + 1];
                    for (int c = 0; c < 4; ++c)
                        sum[c] += w * p[c];
                }
            }
            for (int c = 0; c < 4; ++c)
                out[c] = (unsigned char)(int)(sum[c] + 0.5f);
            out += 4;
        }
    }
    delete[] m_pData;
    m_pData = data;
}
#endif

} // namespace core
