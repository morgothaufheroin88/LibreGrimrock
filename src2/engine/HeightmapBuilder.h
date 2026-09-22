// Terrain tile tessellation of Legend of Grimrock 2, reconstructed from grimrock2.exe
// HeightmapBuilder (0x0042f3d0-0x0042fb80): Lua feeds it the tiles of a level and turns
// the result into the heightmap mesh.
#pragma once
#include "core/Array.h"
#include "core/Vector.h"

namespace core
{
class Image;
}

namespace engine
{

class Mesh;

// 0x30 bytes
class HeightmapBuilder
{
  public:
    static constexpr float TileSize = 3.0f;
    static constexpr int MapHeightTiles = 32; // tile rows run from z = 32 * 3 downwards
    static constexpr float HeightScale = 1.0f / 42.0f;
    static constexpr float HeightBias = 128.0f;
    static constexpr float NoiseAmplitude = 0.1f;

    // 0x0042f3d0: a (subdivisions x subdivisions) grid of quads for tile (x, y) at the
    // elevation, heights sampled from the height map image (nil = flat), vertex colours
    // from the blend image (nil = black), and noise added where the bilinear blend of the
    // corner weights is positive.
    void tessellateTile(int x, int y, int elevation, int subdivisions, const core::Image* heightmap,
                        const core::Image* blendMap, float w00, float w10, float w01, float w11);
    // 0x0042fad0: positions, texcoords, colours and indices as a new mesh
    Mesh* createMesh() const;
    int getNumIndices() const
    {
        return m_indices.size();
    }

  private:
    core::Array<core::Vec3> m_positions;
    core::Array<core::Vec2> m_texcoords;
    core::Array<unsigned int> m_colors;
    core::Array<int> m_indices;
};

} // namespace engine
