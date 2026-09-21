// Reconstructed from Grimrock.bin.x86 DDSLoader.cpp.
#include "engine/DDSLoader.h"
#include "core/Exception.h"
#include "core/Image.h"
#include <cstring>

namespace engine
{

using namespace core;

// 0x080ec5d0
DDSLoader::DDSLoader(const char* filename) : m_pStream(0)
{
    m_pStream = new FileInputStream(filename);
    unsigned int magic;
    m_pStream->readInt(magic);
    if (magic != Magic)
        throw Exception("Not a valid DDS file: %s", filename);
    m_pStream->readBytes(&m_header, sizeof(m_header));
    if (m_header.size != HeaderSize)
        throw Exception("Invalid DDS header size: %s", filename);
    if ((m_header.pfFlags & PixelFormatFourCC) && m_header.fourCC == FourCCDX10)
        throw Exception("Unsupported DX10 DDS file: %s", filename);
    // The original assigns the cube map type and then overwrites it; volume vs 2D only.
    if (m_header.caps2 & Caps2CubeMap)
        m_type = TextureCube;
    m_type = (m_header.caps2 & Caps2Volume) ? Texture3D : Texture2D;
    m_format = 0;
    if (m_header.pfFlags & PixelFormatFourCC)
    {
        switch (m_header.fourCC)
        {
        case FormatDXT1:
        case FormatDXT2:
        case FormatDXT3:
        case FormatDXT4:
        case FormatDXT5:
            m_format = m_header.fourCC;
            break;
        default:
            throw Exception("Unknown four-cc DDS format 0x%08x in file %s", m_header.fourCC,
                            filename);
        }
    }
    else if (m_header.pfFlags & PixelFormatRGB)
    {
        if (m_header.rgbBitCount != 32 || m_header.rBitMask != PixelMaskRed ||
            m_header.gBitMask != PixelMaskGreen || m_header.bBitMask != PixelMaskBlue ||
            m_header.aBitMask != PixelMaskAlpha)
            throw Exception("Unknown uncompressed DDS pixel format in file %s", filename);
        m_format = FormatA8R8G8B8;
    }
    memset(m_surfaceOffsets, 0, sizeof(m_surfaceOffsets));
    memset(m_surfaceSizes, 0, sizeof(m_surfaceSizes));
    int width = (int)m_header.width, height = (int)m_header.height;
    int offset = m_pStream->getPosition();
    for (int i = 0; hasMipMaps() && i < (int)m_header.mipMapCount && i < MaxSurfaces; ++i)
    {
        m_surfaceOffsets[i] = offset;
        m_surfaceSizes[i] = getSurfaceSize(m_format, width, height);
        offset += m_surfaceSizes[i];
        width = width / 2 > 0 ? width / 2 : 1;
        height = height / 2 > 0 ? height / 2 : 1;
    }
}
// 0x080ec3f0
DDSLoader::~DDSLoader()
{
    delete m_pStream;
}
// 0x080ec440
void DDSLoader::loadSurface(int surface, void* data)
{
    m_pStream->seek(m_surfaceOffsets[surface]);
    m_pStream->readBytes(data, m_surfaceSizes[surface]);
}
// 0x080ec4d0
int DDSLoader::getSurfaceSize(unsigned int format, int width, int height)
{
    switch (format)
    {
    case FormatA8R8G8B8:
    case FormatX8R8G8B8:
        return width * 4 * height;
    case FormatDXT1:
        return ((width + 3) / 4) * ((height + 3) / 4) * 8;
    case FormatDXT2:
    case FormatDXT3:
    case FormatDXT4:
    case FormatDXT5:
        return ((width + 3) / 4) * ((height + 3) / 4) * 16;
    default:
        throw Exception("Could not determine DDS texture surface size");
    }
}

} // namespace engine
