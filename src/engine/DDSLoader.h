// DDS texture reader, from DDSLoader.cpp (0x080ec3f0-0x080ec5d0).
#pragma once
#include "core/FileStream.h"

namespace engine
{

class DDSLoader
{
  public:
    enum Type
    {
        Texture2D = 3,
        Texture3D = 4,
        TextureCube = 5
    };
    enum Format
    {
        FormatA8R8G8B8 = 0x15, // D3DFMT_A8R8G8B8
        FormatX8R8G8B8 = 0x16, // D3DFMT_X8R8G8B8
        // Grimrock 2 (grimrock2.exe 0x004f3a40) also reads the 16/32 bit formats
        FormatG16R16 = 0x22,        // D3DFMT_G16R16
        FormatA16B16G16R16 = 0x24,  // D3DFMT_A16B16G16R16
        FormatR16F = 0x6f,          // D3DFMT_R16F
        FormatG16R16F = 0x70,       // D3DFMT_G16R16F
        FormatA16B16G16R16F = 0x71, // D3DFMT_A16B16G16R16F
        FormatR32F = 0x72,          // D3DFMT_R32F
        FormatG32R32F = 0x73,       // D3DFMT_G32R32F
        FormatA32B32G32R32F = 0x74, // D3DFMT_A32B32G32R32F
        FormatDXT1 = 0x31545844,    // "DXT1"
        FormatDXT2 = 0x32545844,    // "DXT2"
        FormatDXT3 = 0x33545844,    // "DXT3"
        FormatDXT4 = 0x34545844,    // "DXT4"
        FormatDXT5 = 0x35545844     // "DXT5"
    };
    // DDS_HEADER / DDS_PIXELFORMAT / DDSCAPS2 bits used by the loader.
    static constexpr unsigned int Magic = 0x20534444;      // "DDS "
    static constexpr unsigned int FourCCDX10 = 0x30315844; // "DX10"
    static constexpr unsigned int HeaderSize = 124;
    static constexpr unsigned int HeaderFlagMipMapCount = 0x20000;
    static constexpr unsigned int HeaderFlagDepth = 0x800000;
    static constexpr unsigned int PixelFormatFourCC = 0x4;
    static constexpr unsigned int PixelFormatRGB = 0x40;
    static constexpr unsigned int Caps2CubeMap = 0x200;
    static constexpr unsigned int Caps2Volume = 0x200000;
    struct Header
    { // 124 bytes, straight from the file
        unsigned int size;
        unsigned int flags;
        unsigned int height;
        unsigned int width;
        unsigned int pitchOrLinearSize;
        unsigned int depth;
        unsigned int mipMapCount;
        unsigned int reserved1[11];
        unsigned int pfSize;
        unsigned int pfFlags;
        unsigned int fourCC;
        unsigned int rgbBitCount;
        unsigned int rBitMask;
        unsigned int gBitMask;
        unsigned int bBitMask;
        unsigned int aBitMask;
        unsigned int caps;
        unsigned int caps2;
        unsigned int caps3;
        unsigned int caps4;
        unsigned int reserved2;
    };
    static constexpr int MaxSurfaces = 16;

    explicit DDSLoader(const char* filename);
    ~DDSLoader();
    int getWidth() const
    {
        return (int)m_header.width;
    }
    int getHeight() const
    {
        return (int)m_header.height;
    }
    bool hasMipMaps() const
    {
        return (m_header.flags & HeaderFlagMipMapCount) != 0;
    }
    int getMipMapCount() const
    {
        return (int)m_header.mipMapCount;
    }
    // Slices of a volume texture, 1 otherwise (grimrock2.exe 0x004f3b90).
    int getDepth() const
    {
        return (m_header.flags & HeaderFlagDepth) ? (int)m_header.depth : 1;
    }
    int getType() const
    {
        return m_type;
    }
    unsigned int getFormat() const
    {
        return m_format;
    }
    int getSurfaceSize(int surface) const
    {
        return m_surfaceSizes[surface];
    }
    void loadSurface(int surface, void* data);
    static int getSurfaceSize(unsigned int format, int width, int height);

  private:
    core::FileInputStream* m_pStream;
    Header m_header;
    int m_type;
    unsigned int m_format;
    int m_surfaceOffsets[MaxSurfaces];
    int m_surfaceSizes[MaxSurfaces];
};

} // namespace engine
