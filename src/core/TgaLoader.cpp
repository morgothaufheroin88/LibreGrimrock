// Reconstructed from Grimrock.bin.x86 TgaLoader.cpp.
#include "core/TgaLoader.h"
#include "core/Exception.h"
#include <cstring>

namespace core
{

constexpr unsigned Mask5Bits = 31;
constexpr unsigned Argb1555AlphaBit = 0x8000;
constexpr unsigned char RlePacketCountMask = 127;
constexpr unsigned char RlePacketRunBit = 128;
constexpr unsigned char DescriptorTopLeftOrigin = 0x20;

static inline unsigned char expand5(unsigned v)
{
    return (unsigned char)((v << 3) | (v >> 2));
}

// 0x080b4cd0
TgaLoader::TgaLoader(InputStream& stream) : m_width(0), m_height(0), m_pData(0), m_pRaw(0)
{
    memset(m_palette, 0, sizeof(m_palette));
    readHeader(stream);
    readColorMap(stream);
    readData(stream);
}
// 0x080b4100
TgaLoader::~TgaLoader()
{
    if (m_pData)
        delete[] m_pData;
    if (m_pRaw)
        delete[] m_pRaw;
}

// 0x080b4660
void TgaLoader::readHeader(InputStream& stream)
{
    unsigned short xOrigin, yOrigin, width, height;
    stream.readByte(m_idLength);
    stream.readByte(m_colorMapType);
    stream.readByte(m_imageType);
    stream.readShort(m_colorMapStart);
    stream.readShort(m_colorMapLength);
    stream.readByte(m_colorMapDepth);
    stream.readShort(xOrigin);
    stream.readShort(yOrigin);
    stream.readShort(width);
    stream.readShort(height);
    stream.readByte(m_pixelDepth);
    stream.readByte(m_descriptor);
    m_width = width;
    m_height = height;
    if (m_width == 0 || m_height == 0)
        throw Exception("Illegal image dimensions");
    bool indexed = m_imageType == 1 || m_imageType == 9;
    bool trueColor = m_imageType == 2 || m_imageType == 10;
    bool mono = m_imageType == 3 || m_imageType == 11;
    if (!indexed && !trueColor && !mono)
        throw Exception("Unsupported tga format");
    if (indexed && (m_colorMapType != 1 || m_colorMapStart + m_colorMapLength > 256))
        throw Exception("Colormapped TGA image has no valid palette");
    if (trueColor && m_pixelDepth != 15 && m_pixelDepth != 16 && m_pixelDepth != 24 &&
        m_pixelDepth != 32)
        throw Exception("Truecolor image is not 15/16/24/32 bits/pixel");
    if (mono && m_pixelDepth != 8)
        throw Exception("Mono image is not 8 bits/pixel");
    m_bytesPerPixel = indexed ? 1 : (m_pixelDepth + 7) / 8;
    stream.skip(m_idLength);
}

// 0x080b43e0
void TgaLoader::readColorMap(InputStream& stream)
{
    if (!m_colorMapType)
        return;
    int bytes = (m_colorMapDepth + 7) / 8;
    if (bytes < 1 || bytes > 4)
        throw Exception("Bad colormap depth");
    for (int i = 0; i < m_colorMapLength; ++i)
    {
        unsigned char e[4] = {0, 0, 0, 255};
        if (bytes == 1)
        {
            unsigned char v;
            stream.readByte(v);
            e[0] = e[1] = e[2] = v;
        }
        else if (bytes == 2)
        {
            unsigned short v;
            stream.readShort(v);
            e[0] = expand5(v & Mask5Bits);
            e[1] = expand5((v >> 5) & Mask5Bits);
            e[2] = expand5((v >> 10) & Mask5Bits);
            e[3] = (v & Argb1555AlphaBit) ? 255 : 0;
        }
        else
        {
            stream.readBytes(e, bytes);
            if (bytes == 3)
                e[3] = 255;
        }
        unsigned int packed;
        memcpy(&packed, e, 4);
        m_palette[m_colorMapStart + i] = packed;
    }
}

// 0x080b4290
void TgaLoader::readRLEPixel(InputStream& stream, unsigned char* dst, int bytesPerPixel)
{
    stream.readBytes(dst, bytesPerPixel);
}

// 0x080b48e0: one packet's worth of raw or run-length pixels.
void TgaLoader::readSpan(InputStream& stream, unsigned char* dst, int count, int bytesPerPixel)
{
    if (m_imageType >= 9)
    {
        int pixel = 0;
        while (pixel < count)
        {
            unsigned char packet;
            stream.readByte(packet);
            int run = (packet & RlePacketCountMask) + 1;
            if (run > count - pixel)
                throw Exception("Bad tga RLE packet");
            if (packet & RlePacketRunBit)
            {
                unsigned char value[4];
                readRLEPixel(stream, value, bytesPerPixel);
                for (int i = 0; i < run; ++i)
                    memcpy(dst + (pixel + i) * bytesPerPixel, value, bytesPerPixel);
            }
            else
            {
                stream.readBytes(dst + pixel * bytesPerPixel, run * bytesPerPixel);
            }
            pixel += run;
        }
    }
    else
    {
        stream.readBytes(dst, count * bytesPerPixel);
    }
}

// 0x080b4140 grayscale
void TgaLoader::imgDequantizeI8(unsigned int* dst)
{
    for (int i = 0; i < m_width * m_height; ++i)
    {
        unsigned v = m_pRaw[i];
        dst[i] = v | (v << 8) | (v << 16) | 0xff000000u;
    }
}
// 0x080b41a0 palette
void TgaLoader::imgDequantizeP8(unsigned int* dst)
{
    for (int i = 0; i < m_width * m_height; ++i)
        dst[i] = m_palette[m_pRaw[i]] | 0xff000000u;
}
// 0x080b41f0
void TgaLoader::imgDequantizeARGB1555(unsigned int* dst)
{
    for (int i = 0; i < m_width * m_height; ++i)
    {
        unsigned short v = (unsigned short)(m_pRaw[i * 2] | (m_pRaw[i * 2 + 1] << 8));
        unsigned char e[4] = {expand5(v & Mask5Bits), expand5((v >> 5) & Mask5Bits),
                              expand5((v >> 10) & Mask5Bits),
                              (unsigned char)((v & Argb1555AlphaBit) ? 255 : 0)};
        memcpy(&dst[i], e, 4);
    }
}

// 0x080b4a60
void TgaLoader::readData(InputStream& stream)
{
    int count = m_width * m_height;
    m_pRaw = new unsigned char[(size_t)count * m_bytesPerPixel];
    readSpan(stream, m_pRaw, count, m_bytesPerPixel);
    unsigned int* pixels = new unsigned int[count];
    if (m_imageType == 1 || m_imageType == 9)
        imgDequantizeP8(pixels);
    else if (m_imageType == 3 || m_imageType == 11)
        imgDequantizeI8(pixels);
    else if (m_pixelDepth == 15 || m_pixelDepth == 16)
        imgDequantizeARGB1555(pixels);
    else if (m_pixelDepth == 24)
    {
        for (int i = 0; i < count; ++i)
        {
            unsigned char e[4] = {m_pRaw[i * 3], m_pRaw[i * 3 + 1], m_pRaw[i * 3 + 2], 255};
            memcpy(&pixels[i], e, 4);
        }
    }
    else
    {
        memcpy(pixels, m_pRaw, (size_t)count * 4);
    }
    delete[] m_pRaw;
    m_pRaw = 0;
    // Rows are stored bottom-up unless the top-left origin bit of the descriptor is set.
    m_pData = new unsigned char[(size_t)count * 4];
    for (int y = 0; y < m_height; ++y)
    {
        int srcY = (m_descriptor & DescriptorTopLeftOrigin) ? y : m_height - 1 - y;
        memcpy(m_pData + (size_t)y * m_width * 4, pixels + (size_t)srcY * m_width,
               (size_t)m_width * 4);
    }
    delete[] pixels;
}

} // namespace core
