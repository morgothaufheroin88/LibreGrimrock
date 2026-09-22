#include "GameIcon.h"
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb/stb_image.h"

namespace launcher
{

namespace
{

constexpr unsigned ResourceIcon = 3;
constexpr unsigned ResourceGroupIcon = 14;
constexpr unsigned IconGroup = 0x65;

typedef std::vector<unsigned char> Bytes;

unsigned read16(const Bytes& data, size_t offset)
{
    return offset + 2 <= data.size() ? data[offset] | (data[offset + 1] << 8) : 0;
}
unsigned read32(const Bytes& data, size_t offset)
{
    return offset + 4 <= data.size() ? read16(data, offset) | (read16(data, offset + 2) << 16) : 0;
}

// The resource directory of a PE file: type -> id -> language -> data.
class Resources
{
  public:
    explicit Resources(const Bytes& file) : m_file(file), m_base(0), m_address(0)
    {
        size_t pe = read32(file, 0x3c);
        if (pe + 24 > file.size() || memcmp(&file[pe], "PE\0\0", 4) != 0)
            return;
        unsigned sections = read16(file, pe + 6);
        size_t table = pe + 24 + read16(file, pe + 20);
        for (unsigned i = 0; i < sections; ++i)
        {
            size_t section = table + i * 40;
            if (section + 40 > file.size())
                return;
            if (memcmp(&file[section], ".rsrc", 6) == 0)
            {
                m_address = read32(file, section + 12);
                m_base = read32(file, section + 20);
                return;
            }
        }
    }
    // the data of the first language of resource (type, id); id 0 = the first of the type
    Bytes find(unsigned type, unsigned id) const
    {
        if (!m_base)
            return {};
        size_t typeEntry = entry(0, type);
        if (!typeEntry)
            return {};
        size_t idEntry = entry(typeEntry, id);
        if (!idEntry)
            return {};
        size_t languageEntry = entry(idEntry, 0);
        size_t leaf = languageEntry ? languageEntry : idEntry;
        size_t address = read32(m_file, m_base + leaf);
        size_t size = read32(m_file, m_base + leaf + 4);
        size_t start = m_base + address - m_address;
        if (start + size > m_file.size())
            return {};
        return Bytes(m_file.begin() + start, m_file.begin() + start + size);
    }

  private:
    // the offset of the subdirectory or leaf that entry id of a directory points to
    size_t entry(size_t directory, unsigned id) const
    {
        size_t at = m_base + directory;
        unsigned count = read16(m_file, at + 12) + read16(m_file, at + 14);
        for (unsigned i = 0; i < count; ++i)
        {
            unsigned key = read32(m_file, at + 16 + i * 8);
            unsigned value = read32(m_file, at + 20 + i * 8);
            if (id == 0 || (key & 0x7fffffff) == id)
                return value & 0x7fffffff;
        }
        return 0;
    }

    const Bytes& m_file;
    size_t m_base;
    size_t m_address;
};

SDL_Surface* surfaceFromPng(const unsigned char* data, size_t size)
{
    int width = 0, height = 0, channels = 0;
    unsigned char* pixels = stbi_load_from_memory(data, (int)size, &width, &height, &channels, 4);
    if (!pixels)
        return nullptr;
    SDL_Surface* surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32);
    for (int y = 0; y < height; ++y)
        memcpy((Uint8*)surface->pixels + y * surface->pitch, pixels + y * width * 4, width * 4);
    stbi_image_free(pixels);
    return surface;
}

// a 32 bit icon image: a bitmap header, then BGRA rows bottom up (and a mask after them)
SDL_Surface* surfaceFromIconBitmap(const Bytes& icon)
{
    unsigned headerSize = read32(icon, 0);
    int width = (int)read32(icon, 4);
    int height = (int)read32(icon, 8) / 2; // the colour and the mask image are stacked
    unsigned bits = read16(icon, 14);
    if (bits != 32 || width <= 0 || height <= 0 ||
        headerSize + (size_t)width * height * 4 > icon.size())
        return nullptr;
    SDL_Surface* surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32);
    for (int y = 0; y < height; ++y)
    {
        const unsigned char* source = &icon[headerSize + (size_t)(height - 1 - y) * width * 4];
        Uint8* target = (Uint8*)surface->pixels + y * surface->pitch;
        for (int x = 0; x < width; ++x)
        {
            target[x * 4 + 0] = source[x * 4 + 2];
            target[x * 4 + 1] = source[x * 4 + 1];
            target[x * 4 + 2] = source[x * 4 + 0];
            target[x * 4 + 3] = source[x * 4 + 3];
        }
    }
    return surface;
}

// the largest icon of the group the Windows build loads
SDL_Surface* surfaceFromExecutable(const std::string& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        return nullptr;
    Bytes file((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    Resources resources(file);
    Bytes group = resources.find(ResourceGroupIcon, IconGroup);
    unsigned count = read16(group, 4);
    unsigned bestId = 0, bestWidth = 0;
    for (unsigned i = 0; i < count; ++i)
    {
        size_t entry = 6 + i * 14;
        unsigned width = group.size() > entry ? group[entry] : 0;
        if (width == 0)
            width = 256; // 0 stands for 256
        if (width > bestWidth)
        {
            bestWidth = width;
            bestId = read16(group, entry + 12);
        }
    }
    if (!bestId)
        return nullptr;
    Bytes icon = resources.find(ResourceIcon, bestId);
    if (icon.size() > 8 && memcmp(icon.data(), "\x89PNG", 4) == 0)
        return surfaceFromPng(icon.data(), icon.size());
    return surfaceFromIconBitmap(icon);
}

} // namespace

SDL_Texture* loadGameIcon(SDL_Renderer* renderer, const GameInfo& game,
                          const std::string& directory)
{
    if (directory.empty())
        return nullptr;
    SDL_Surface* surface = nullptr;
    std::ifstream png(directory + "/" + game.iconFile, std::ios::binary);
    if (png)
    {
        Bytes data((std::istreambuf_iterator<char>(png)), std::istreambuf_iterator<char>());
        surface = surfaceFromPng(data.data(), data.size());
    }
    if (!surface && game.windowsExe)
        surface = surfaceFromExecutable(directory + "/" + game.windowsExe);
    if (!surface)
        return nullptr;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
    SDL_DestroySurface(surface);
    return texture;
}

} // namespace launcher
