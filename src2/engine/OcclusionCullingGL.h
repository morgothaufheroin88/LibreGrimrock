// Software occlusion culling of Legend of Grimrock 2, reconstructed from grimrock2.exe
// OcclusionCulling.cpp (0x004d0120-0x004d16e0): the occluder meshes are rasterised into
// a 128x70 depth buffer on the CPU and the entity bounds are tested against it.
#pragma once
#include "core/Array.h"
#include "core/Matrix.h"
#include "core/SharedPtr.h"

namespace engine
{

class Camera;
class Mesh;
class RenderEntity;
class LightEntity;
class OccluderEntity;

// 0x20 bytes
class OcclusionCullingGL
{
  public:
    static constexpr int BufferWidth = 128;
    static constexpr int BufferHeight = 70;
    static constexpr int NumTestMatrices = 1000;
    // 0x004d10d0
    OcclusionCullingGL();
    // 0x004d1240
    ~OcclusionCullingGL();
    // 0x004d12d0: clears the depth buffer and rasterises the occluders, clipping the
    // ones crossing the near plane.
    void renderOccluders(OccluderEntity* const* occluders, int count, const Camera& camera);
    // 0x004d06a0: keeps the entities whose bounds are not hidden behind the buffer;
    // returns the number kept, compacted to the front of the array.
    int testAABBs(RenderEntity** entities, int count, const Camera& camera);
    // 0x004d0b10: bit mask of the cube faces of a point light whose bounds show
    unsigned int testPointLight(const LightEntity& light, const Camera& camera);
    const float* getDepthBuffer() const
    {
        return m_pDepthBuffer;
    }

  private:
    // 0x004d0230: min depth rasterisation of a screen space triangle
    void rasterizeTriangle(const core::Vec3* vertices);
    // 0x004d0120: clips a polygon against z = nearZ in view space, returns the vertex count
    static int clipPolygon(const core::Vec3* in, int count, float nearZ, core::Vec3* out);
    bool testRect(float minX, float minY, float maxX, float maxY, float minZ) const;

    float* m_pDepthBuffer;
    core::Array<core::Vec3> m_transformedVertices;
    core::SharedPtr<Mesh> m_testMesh;
    core::Array<core::Matrix4x3> m_testMatrices;
};

} // namespace engine
