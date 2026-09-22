// Reconstructed from grimrock2.exe OcclusionCulling.cpp.
#include "engine/OcclusionCullingGL.h"
#include "core/Profiler.h"
#include "engine/Camera.h"
#include "engine/Mesh.h"
#include "engine/RenderEntity.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdlib>

namespace engine
{

using namespace core;

constexpr float HalfWidth = OcclusionCullingGL::BufferWidth / 2.0f;
constexpr float HalfHeight = OcclusionCullingGL::BufferHeight / 2.0f;

// 0x004d10d0: a unit box and 1000 random transforms of it were the original test scene.
OcclusionCullingGL::OcclusionCullingGL() : m_pDepthBuffer(0)
{
    m_pDepthBuffer = new float[BufferWidth * BufferHeight];
    m_transformedVertices.reserve(0x2000 / (int)sizeof(Vec3));
    m_testMesh.reset(createBox(2.0f, 2.0f, 2.0f));
    m_testMatrices.reserve(NumTestMatrices);
    for (int i = 0; i < NumTestMatrices; ++i)
    {
        Matrix4x3 m;
        m.makeIdentity();
        m.pos.x = (float)rand() / (float)RAND_MAX * 200.0f - 100.0f;
        m.pos.z = (float)rand() / (float)RAND_MAX * 200.0f - 100.0f;
        m_testMatrices.push_back(m);
    }
}
// 0x004d1240
OcclusionCullingGL::~OcclusionCullingGL()
{
    delete[] m_pDepthBuffer;
}

// 0x004d0120: Sutherland-Hodgman against the plane z = nearZ (view space z grows away
// from the camera).
int OcclusionCullingGL::clipPolygon(const Vec3* in, int count, float nearZ, Vec3* out)
{
    int n = 0;
    for (int i = 0; i < count; ++i)
    {
        const Vec3& a = in[(i + count - 1) % count];
        const Vec3& b = in[i];
        bool aIn = a.z >= nearZ, bIn = b.z >= nearZ;
        if (aIn != bIn)
        {
            float t = (nearZ - a.z) / (b.z - a.z);
            out[n++] = Vec3(a.x + t * (b.x - a.x), a.y + t * (b.y - a.y), nearZ);
        }
        if (bIn)
            out[n++] = b;
    }
    return n;
}

// 0x004d0230: half space rasterisation in integer pixel coordinates; a pixel keeps the
// nearest depth. Back facing triangles (clockwise on screen) are skipped.
void OcclusionCullingGL::rasterizeTriangle(const Vec3* v)
{
    int x[3], y[3];
    for (int i = 0; i < 3; ++i)
    {
        x[i] = (int)(v[i].x);
        y[i] = (int)(v[i].y);
    }
    int area = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0]);
    if (area <= 0)
        return;
    float invArea = 1.0f / (float)area;
    int minX = std::min(x[0], std::min(x[1], x[2]));
    int maxX = std::max(x[0], std::max(x[1], x[2]));
    int minY = std::min(y[0], std::min(y[1], y[2]));
    int maxY = std::max(y[0], std::max(y[1], y[2]));
    if (minX < 0)
        minX = 0;
    if (minY < 0)
        minY = 0;
    if (maxX > BufferWidth)
        maxX = BufferWidth;
    if (maxY > BufferHeight)
        maxY = BufferHeight;
    for (int py = minY; py < maxY; ++py)
    {
        for (int px = minX; px < maxX; ++px)
        {
            int w0 = (x[1] - px) * (y[2] - py) - (x[2] - px) * (y[1] - py);
            int w1 = (x[2] - px) * (y[0] - py) - (x[0] - px) * (y[2] - py);
            int w2 = (x[0] - px) * (y[1] - py) - (x[1] - px) * (y[0] - py);
            if ((w0 | w1 | w2) < 0)
                continue;
            float z = (w0 * v[0].z + w1 * v[1].z + w2 * v[2].z) * invArea;
            float& depth = m_pDepthBuffer[py * BufferWidth + px];
            if (z < depth)
                depth = z;
        }
    }
}

static inline Vec3 toScreen(const Vec4& clip)
{
    float invW = 1.0f / clip.w;
    return Vec3(clip.x * invW * HalfWidth + HalfWidth, HalfHeight - clip.y * invW * HalfHeight,
                clip.z * invW);
}

// 0x004d12d0
void OcclusionCullingGL::renderOccluders(OccluderEntity* const* occluders, int count,
                                         const Camera& camera)
{
    ProfileScope profile("_RenderOccluders");
    for (int i = 0; i < BufferWidth * BufferHeight; ++i)
        m_pDepthBuffer[i] = 1.0f;
    float nearZ = camera.getNear();
    const Matrix4x3& worldToView = camera.getWorldToLocalMatrix();
    const Matrix4x4& viewProj = camera.getViewProjectionMatrix();
    const Matrix4x4& proj = camera.getProjectionMatrix();
    for (int o = 0; o < count; ++o)
    {
        const OccluderEntity& occluder = *occluders[o];
        Mesh* mesh = occluder.getMesh();
        const Matrix4x3& localToWorld = occluder.getNode()->getLocalToWorldMatrix();
        const Vec3* pos = (const Vec3*)mesh->getVertexArray(Mesh::Position);
        const Array<int>& idx = mesh->getIndices();
        int numVertices = mesh->getNumVertices();
        m_transformedVertices.resize(numVertices);
        float viewZ = worldToView.transformPoint(localToWorld.pos).z;
        if (viewZ - occluder.getRadius() > nearZ)
        {
            // entirely beyond the near plane: straight to the screen
            Matrix4x4 mvp = viewProj * Matrix4x4(localToWorld);
            for (int i = 0; i < numVertices; ++i)
                m_transformedVertices[i] = toScreen(mvp.transform(Vec4(pos[i], 1.0f)));
            for (int t = 0; t + 2 < idx.size(); t += 3)
            {
                Vec3 tri[3] = {m_transformedVertices[idx[t]], m_transformedVertices[idx[t + 1]],
                               m_transformedVertices[idx[t + 2]]};
                rasterizeTriangle(tri);
            }
        }
        else
        {
            // clip each triangle in view space first
            Matrix4x3 modelView = worldToView * localToWorld;
            for (int i = 0; i < numVertices; ++i)
                m_transformedVertices[i] = modelView.transformPoint(pos[i]);
            for (int t = 0; t + 2 < idx.size(); t += 3)
            {
                Vec3 tri[3] = {m_transformedVertices[idx[t]], m_transformedVertices[idx[t + 1]],
                               m_transformedVertices[idx[t + 2]]};
                Vec3 poly[8];
                int n = clipPolygon(tri, 3, nearZ, poly);
                for (int i = 0; i < n; ++i)
                    poly[i] = toScreen(proj.transform(Vec4(poly[i], 1.0f)));
                if (n >= 3)
                    rasterizeTriangle(poly);
                if (n >= 4)
                {
                    Vec3 second[3] = {poly[0], poly[2], poly[3]};
                    rasterizeTriangle(second);
                }
            }
        }
    }
}

// Any pixel of the rectangle (in screen coordinates, inclusive) farther than minZ means
// something of the box shows.
bool OcclusionCullingGL::testRect(float minX, float minY, float maxX, float maxY, float minZ) const
{
    int x0 = (int)lrintf(minX) - 1, x1 = (int)lrintf(maxX) + 1;
    int y0 = (int)lrintf(minY) - 1, y1 = (int)lrintf(maxY) + 1;
    x0 = x0 < 0 ? 0 : (x0 > BufferWidth - 1 ? BufferWidth - 1 : x0);
    x1 = x1 < 0 ? 0 : (x1 > BufferWidth - 1 ? BufferWidth - 1 : x1);
    y0 = y0 < 0 ? 0 : (y0 > BufferHeight - 1 ? BufferHeight - 1 : y0);
    y1 = y1 < 0 ? 0 : (y1 > BufferHeight - 1 ? BufferHeight - 1 : y1);
    for (int y = y0; y <= y1; ++y)
    {
        const float* row = m_pDepthBuffer + y * BufferWidth;
        for (int x = x0; x <= x1; ++x)
            if (row[x] > minZ)
                return true;
    }
    return false;
}

// Screen space rectangle of the given points; false when one is behind the camera (the
// box then counts as visible).
static bool projectPoints(const Vec3* points, int count, const Matrix4x4& viewProj, float& minX,
                          float& minY, float& maxX, float& maxY, float& minZ)
{
    minX = minY = minZ = FLT_MAX;
    maxX = maxY = -FLT_MAX;
    for (int i = 0; i < count; ++i)
    {
        Vec4 clip = viewProj.transform(Vec4(points[i], 1.0f));
        if (clip.w <= 0.0f)
            return false;
        Vec3 s = toScreen(clip);
        minX = std::min(minX, s.x);
        minY = std::min(minY, s.y);
        maxX = std::max(maxX, s.x);
        maxY = std::max(maxY, s.y);
        minZ = std::min(minZ, s.z);
    }
    return true;
}
static void boxCorners(const AABox3& box, Vec3* corners)
{
    for (int i = 0; i < 8; ++i)
        corners[i].set((i & 4) ? box.max.x : box.min.x, (i & 2) ? box.max.y : box.min.y,
                       (i & 1) ? box.max.z : box.min.z);
}

// 0x004d06a0
int OcclusionCullingGL::testAABBs(RenderEntity** entities, int count, const Camera& camera)
{
    ProfileScope profile("_OcclusionCullTestAABBs");
    const Matrix4x4& viewProj = camera.getViewProjectionMatrix();
    int kept = 0;
    for (int i = 0; i < count; ++i)
    {
        RenderEntity* entity = entities[i];
        bool visible = true;
        if (!entity->isUnbounded())
        {
            Vec3 corners[8];
            boxCorners(entity->getWorldBounds(), corners);
            float minX, minY, maxX, maxY, minZ;
            if (projectPoints(corners, 8, viewProj, minX, minY, maxX, maxY, minZ))
                visible = testRect(minX, minY, maxX, maxY, minZ);
        }
        if (visible)
            entities[kept++] = entity;
    }
    return kept;
}

// 0x004d0b10: each face of the bounding box plus its centre; a face whose rectangle is
// hidden does not need a shadow map.
unsigned int OcclusionCullingGL::testPointLight(const LightEntity& light, const Camera& camera)
{
    ProfileScope profile("_OcclusionCullTestPointLight");
    const Matrix4x4& viewProj = camera.getViewProjectionMatrix();
    AABox3 box = light.getWorldBounds();
    Vec3 corners[9];
    boxCorners(box, corners);
    corners[8] = (box.min + box.max) * 0.5f;
    static constexpr int faces[6][5] = {{4, 5, 6, 7, 8}, {0, 1, 2, 3, 8}, {2, 3, 6, 7, 8},
                                        {0, 1, 4, 5, 8}, {1, 3, 5, 7, 8}, {0, 2, 4, 6, 8}};
    unsigned int mask = 0;
    for (int f = 0; f < 6; ++f)
    {
        Vec3 points[5];
        for (int i = 0; i < 5; ++i)
            points[i] = corners[faces[f][i]];
        float minX, minY, maxX, maxY, minZ;
        if (!projectPoints(points, 5, viewProj, minX, minY, maxX, maxY, minZ) ||
            testRect(minX, minY, maxX, maxY, minZ))
            mask |= 1u << f;
    }
    return mask;
}

} // namespace engine
