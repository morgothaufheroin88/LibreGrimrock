// Reconstructed from grimrock2.exe HeightmapBuilder.
#include "engine/HeightmapBuilder.h"
#include "core/Image.h"
#include "core/Noise.h"
#include "engine/Mesh.h"
#include <cmath>

namespace engine
{

using namespace core;

// 0x0042f3d0
void HeightmapBuilder::tessellateTile(int x, int y, int elevation, int subdivisions,
                                      const Image* heightmap, const Image* blendMap, float w00,
                                      float w10, float w01, float w11)
{
    int blendSize = blendMap ? blendMap->getWidth() : 0;
    int firstVertex = m_positions.size();
    for (int j = 0; j <= subdivisions; ++j)
    {
        float v = (float)j / (float)subdivisions;
        for (int i = 0; i <= subdivisions; ++i)
        {
            float u = (float)i / (float)subdivisions;
            float height = 0.0f;
            if (heightmap)
            {
                // the height map has two texels per tile, centred on the tile corners
                Vec4 sample = heightmap->sampleLinearClamp(((float)x + u) * 2.0f - 0.5f,
                                                           ((float)y + v) * 2.0f - 0.5f);
                height = (sample.x - HeightBias) * HeightScale;
            }
            float px = (float)x * TileSize + u * TileSize;
            float pz = (float)(MapHeightTiles - y) * TileSize - v * TileSize;
            float weight = w11 * u * v + w10 * u * (1.0f - v) + w00 * (1.0f - u) * (1.0f - v) +
                           w01 * (1.0f - u) * v;
            if (weight > 0.0f)
                height += noise(px, pz) * weight * NoiseAmplitude;
            m_positions.push_back(Vec3(px, height + (float)elevation * TileSize, pz));
            m_texcoords.push_back(Vec2((float)x + u, (float)y + v));
            unsigned int color = 0xff000000;
            if (blendMap)
            {
                Vec4 sample = blendMap->sampleLinearClamp((float)(blendSize - 1) * u,
                                                          (float)(blendSize - 1) * v);
                unsigned int gray = (unsigned int)(int)(sample.x) & 0xff;
                color = 0xff000000 | (gray << 16) | (gray << 8) | gray;
            }
            m_colors.push_back(color);
        }
    }
    int stride = subdivisions + 1;
    for (int j = 0; j < subdivisions; ++j)
    {
        for (int i = 0; i < subdivisions; ++i)
        {
            int base = firstVertex + j * stride + i;
            m_indices.push_back(base);
            m_indices.push_back(base + 1);
            m_indices.push_back(base + stride);
            m_indices.push_back(base + 1);
            m_indices.push_back(base + stride + 1);
            m_indices.push_back(base + stride);
        }
    }
}

// 0x0042fad0
Mesh* HeightmapBuilder::createMesh() const
{
    Mesh* mesh = new Mesh(m_positions.size());
    mesh->setVertexArray(Mesh::Position, Mesh::TypeFloat, 3, m_positions.data());
    mesh->setVertexArray(Mesh::Texcoord0, Mesh::TypeFloat, 2, m_texcoords.data());
    mesh->setVertexArray(Mesh::Color, Mesh::TypeByte, 4, m_colors.data());
    mesh->setIndices(m_indices.data(), m_indices.size());
    return mesh;
}

} // namespace engine
