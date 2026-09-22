// 32-bit BGRA image, reconstructed from Image.cpp (0x080ca660-0x080cb710). The original
// loaded/saved through FreeImage; this build uses stb_image for the same formats.
#pragma once
#include "core/Color.h"
#include "core/Vector.h"

namespace core
{

// Image pixels are 32-bit BGRA in memory (little endian 0xAARRGGBB).
constexpr unsigned int PixelMaskRed = 0x00ff0000;
constexpr unsigned int PixelMaskGreen = 0x0000ff00;
constexpr unsigned int PixelMaskBlue = 0x000000ff;
constexpr unsigned int PixelMaskAlpha = 0xff000000;

class Image
{
  public:
    Image();
    Image(int width, int height);
    explicit Image(const char* filename);
    Image(const Image& other);
    ~Image();
    Image& operator=(const Image& other);

    int getWidth() const
    {
        return m_width;
    }
    int getHeight() const
    {
        return m_height;
    }
    unsigned char* getData()
    {
        return m_pData;
    }
    const unsigned char* getData() const
    {
        return m_pData;
    }
    void clear(const Color& color);
    void setPixel(int x, int y, const Color& color);
    Color getPixel(int x, int y) const;
    void setPixelSafe(int x, int y, const Color& color);
    Color getPixelSafe(int x, int y) const;
    void flipY();
    void copy(int x, int y, const Image* src, int srcX, int srcY, int width, int height);
    // Box filter downsample; halve() also handles non power of two sizes with a 3x3 kernel.
    void halvePow2();
    void halve();
    void save(const char* filename, bool alpha);
#if GRIMROCK_GAME >= 2
    // Grimrock 2 additions (0x004997f0-0x0049a2f0): the samples are RGBA in 0..255 with
    // the coordinates clamped to the edges.
    void fillRect(int x, int y, int width, int height, const Color& color);
    Vec4 sampleNearestClamp(float x, float y) const;
    Vec4 sampleLinearClamp(float x, float y) const;
    // Bilinear rescale to the new size.
    void resample(int width, int height);
    // 3x3 blur: kernel 0 = gentle (0.64 centre), 1 = gaussian (0.25 centre).
    void blur(int kernel);
#endif

  private:
    int m_width;
    int m_height;
    unsigned char* m_pData; // B, G, R, A
};

} // namespace core
