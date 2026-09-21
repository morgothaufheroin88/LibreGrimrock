// TGA decoder, reconstructed from TgaLoader.cpp (0x080b4100-0x080b4cd0). Produces BGRA8,
// top-down rows.
#pragma once
#include "core/Stream.h"

namespace core
{

class TgaLoader
{
  public:
    explicit TgaLoader(InputStream& stream);
    ~TgaLoader();
    int getWidth()
    {
        return m_width;
    }
    int getHeight()
    {
        return m_height;
    }
    unsigned char* getData()
    {
        return m_pData;
    }
    // Releases ownership of the pixel buffer to the caller.
    unsigned char* takeData()
    {
        unsigned char* p = m_pData;
        m_pData = 0;
        return p;
    }

  private:
    void readHeader(InputStream& stream);
    void readColorMap(InputStream& stream);
    void readData(InputStream& stream);
    void readSpan(InputStream& stream, unsigned char* dst, int count, int bytesPerPixel);
    void readRLEPixel(InputStream& stream, unsigned char* dst, int bytesPerPixel);
    void imgDequantizeI8(unsigned int* dst);
    void imgDequantizeP8(unsigned int* dst);
    void imgDequantizeARGB1555(unsigned int* dst);

    int m_width;
    int m_height;
    unsigned char* m_pData;
    unsigned char m_idLength;
    unsigned char m_colorMapType;
    unsigned char m_imageType;
    unsigned short m_colorMapStart;
    unsigned short m_colorMapLength;
    unsigned char m_colorMapDepth;
    unsigned char m_pixelDepth;
    unsigned char m_descriptor;
    unsigned int m_palette[256];
    unsigned char* m_pRaw;
    int m_bytesPerPixel;
};

} // namespace core
