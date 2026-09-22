// Reconstructed from grimrock2.exe RenderEntity.cpp.
#include "engine/RenderEntity.h"
#include "core/DebugDraw.h"
#include "core/Math.h"
#include "engine/Camera.h"
#include "engine/Mesh.h"
#include "engine/Renderer.h"
#include "engine/Scene.h"
#include "engine/SpatialDS.h"
#include "engine/Texture.h"
#include <cmath>

namespace engine
{

using namespace core;

// ---- Skeleton ------------------------------------------------------------------

// 0x004b4620
Skeleton::Skeleton() : m_frame(-1) {}
// 0x004b4650
void Skeleton::addBone(Node& bone, const Matrix4x3& invBindMatrix)
{
    m_bones.push_back(&bone);
    m_invBindMatrices.push_back(invBindMatrix);
}
// 0x004b3af0
void Skeleton::computeSkinningMatrices(const Matrix4x3& worldToModel, Matrix4x3* out) const
{
    for (int i = 0; i < m_bones.size(); ++i)
        out[i] = worldToModel * (m_bones[i]->getLocalToWorldMatrix() * m_invBindMatrices[i]);
}
// 0x004b46b0
void Skeleton::computeSkinningMatrices(const MeshEntity& entity, float* out, int frame)
{
    const Matrix4x3& worldToModel = entity.getNode()->getWorldToLocalMatrix();
    if (m_frame != frame)
    {
        Matrix4x3 identity;
        identity.makeIdentity();
        m_skinningMatrices.resize(m_bones.size(), identity);
        computeSkinningMatrices(worldToModel, m_skinningMatrices.data());
        m_frame = frame;
    }
    for (int i = 0; i < m_bones.size(); ++i)
    {
        const Matrix4x3& m = m_skinningMatrices[i];
        float* row = out + i * 12;
        row[0] = m.x.x;
        row[1] = m.y.x;
        row[2] = m.z.x;
        row[3] = m.pos.x;
        row[4] = m.x.y;
        row[5] = m.y.y;
        row[6] = m.z.y;
        row[7] = m.pos.y;
        row[8] = m.x.z;
        row[9] = m.y.z;
        row[10] = m.z.z;
        row[11] = m.pos.z;
    }
}

// ---- RenderEntity --------------------------------------------------------------

// 0x004b3420
RenderEntity::RenderEntity(EntityType type)
    : Component(RenderEntityComponent), m_spatialIndex(-1), m_unbounded(false), m_entityType(type),
      m_drawBoundBox(false), m_hidden(false), m_sortOffset(0.0f), m_passMask(DefaultPassMask),
      m_renderHack(0)
{
}
RenderEntity::~RenderEntity() {}
// 0x004b3570
void RenderEntity::addedToNode(Node* node)
{
    if (node->getScene())
        node->getScene()->getSpatialDS()->addEntity(*this);
}
// 0x004b35a0
void RenderEntity::removedFromNode()
{
    if (m_pNode->getScene())
        m_pNode->getScene()->getSpatialDS()->removeEntity(*this);
}
// 0x004b35c0
void RenderEntity::notifyBoundChanged()
{
    if (m_pNode && m_pNode->getScene() && m_spatialIndex != -1)
        m_pNode->getScene()->getSpatialDS()->notifyBoundChanged(*this);
}
// 0x004b3460
void RenderEntity::visualize()
{
    DebugDraw::drawBox(getWorldBounds(), Color::White);
}

// ---- MeshEntity ----------------------------------------------------------------

// 0x004b5020
MeshEntity::MeshEntity(RenderableMesh* mesh)
    : RenderEntity(MeshEntityType), m_skinned(false), m_emissiveColor(0, 0, 0), m_flags(CastShadow),
      m_dissolve(0.0f), m_dissolveStart(0.0f), m_dissolveEnd(0.0f), m_shadowMinDistance(0.0f),
      m_shadowMaxDistance(DefaultShadowMaxDistance), m_skinningDistance(DefaultSkinningDistance),
      m_distance(0.0f)
{
    m_bounds.min.set(0, 0, 0);
    m_bounds.max.set(0, 0, 0);
    setMesh(mesh);
}
// 0x004b4ea0
MeshEntity::~MeshEntity() {}
// 0x004b4f40
void MeshEntity::setMesh(RenderableMesh* mesh)
{
    m_mesh.reset(mesh);
    m_materials.clear();
    if (mesh)
        m_materials = mesh->getMaterials();
    m_unbounded = m_mesh.get() == 0;
    notifyBoundChanged();
}
// 0x004b5010
void MeshEntity::setSkeleton(Skeleton* skeleton)
{
    m_skeleton.reset(skeleton);
}
// 0x004b4490
void MeshEntity::setMaterial(Material* material)
{
    for (int i = 0; i < m_materials.size(); ++i)
        m_materials[i].reset(material);
}
// 0x004b4cc0
void MeshEntity::setMaterials(const Array<SharedPtr<Material>>& materials)
{
    if (&m_materials != &materials)
        m_materials = materials;
}
// 0x004b3680
void MeshEntity::setManualBounds(const AABox3& bounds, bool worldSpace)
{
    m_flags |= worldSpace ? ManualWorldBounds : ManualBounds;
    m_bounds = bounds;
    notifyBoundChanged();
}
// 0x004b35f0
AABox3 MeshEntity::getWorldBounds() const
{
    if (m_flags & ManualWorldBounds)
        return m_bounds;
    if (m_flags & ManualBounds)
        return transformBox(m_bounds, m_pNode->getLocalToWorldMatrix());
    return transformBox(m_mesh->getBoundingBox(), m_pNode->getLocalToWorldMatrix());
}
void MeshEntity::visualize()
{
    DebugDraw::drawBox(getWorldBounds(), Color::White);
}

// ---- LightEntity ---------------------------------------------------------------

// 0x004b48d0
LightEntity::LightEntity(LightType type)
    : RenderEntity(LightEntityType), m_visibleFaces(0), m_lightType(type), m_color(1, 1, 1),
      m_color2(0, 0, 0), m_color3(0, 0, 0), m_range(1.0f), m_spotAngle(HALF_PI),
      m_spotSharpness(0.0f), m_castShadow(false), m_maxShadowDistance(DefaultMaxShadowDistance),
      m_shadowMapSize(DefaultShadowMapSize), m_primaryLight(true), m_specular(true),
      m_debugDraw(false)
{
    m_unbounded = type < Point;
    notifyBoundChanged();
    for (int i = 0; i < NumClipDistances; ++i)
        m_clipDistances[i] = DefaultClipDistance;
}
// 0x004b4cd0
LightEntity::~LightEntity() {}
// 0x004b36f0
void LightEntity::setLightType(LightType type)
{
    m_lightType = type;
    m_unbounded = type < Point;
    notifyBoundChanged();
}
// 0x004b3740
void LightEntity::setLightRange(float range)
{
    m_range = range;
    notifyBoundChanged();
}
// 0x004b3780
void LightEntity::setSpotAngle(float angle)
{
    m_spotAngle = angle;
    notifyBoundChanged();
}
// 0x004b37c0
void LightEntity::setClipDistance(int face, float distance)
{
    m_clipDistances[face] = distance;
    notifyBoundChanged();
}
// 0x004b4a00
void LightEntity::setStaticShadowMap(RenderableTexture* texture)
{
    m_staticShadowMap.reset(texture);
}
// 0x004b4a10
RenderableTexture* LightEntity::renderStaticShadowMap(int size, float bias, int flags,
                                                      bool updateClipDistances)
{
    float extents[NumClipDistances];
    RenderableTexture* texture =
        Renderer::sm_pActiveRenderer->renderStaticShadowMap(*this, size, bias, flags, extents);
    if (updateClipDistances)
    {
        int faces = m_lightType == Point ? NumClipDistances : 1;
        for (int i = 0; i < faces; ++i)
        {
            if (m_clipDistances[i] > 0.0f)
            {
                m_clipDistances[i] = extents[i];
                notifyBoundChanged();
            }
        }
    }
    return texture;
}
// 0x004b4100: the six faces of a point light are frusta of 90 degrees reaching as far as
// the clip distance (at most the range); a face whose frustum misses all camera planes
// is not rendered.
unsigned int LightEntity::getVisibleFaceMask(const Camera& camera) const
{
    // 0x0061cd70: the rotations of the cube faces
    static const Matrix3x3 faceRotations[NumClipDistances] = {
        Matrix3x3(Vec3(0, 0, -1), Vec3(0, 1, 0), Vec3(1, 0, 0)),
        Matrix3x3(Vec3(0, 0, 1), Vec3(0, 1, 0), Vec3(-1, 0, 0)),
        Matrix3x3(Vec3(1, 0, 0), Vec3(0, 0, -1), Vec3(0, 1, 0)),
        Matrix3x3(Vec3(1, 0, 0), Vec3(0, 0, 1), Vec3(0, -1, 0)),
        Matrix3x3(Vec3(1, 0, 0), Vec3(0, 1, 0), Vec3(0, 0, 1)),
        Matrix3x3(Vec3(-1, 0, 0), Vec3(0, 1, 0), Vec3(0, 0, -1))};
    unsigned int mask = (1u << NumClipDistances) - 1;
    Matrix4x3 m;
    m.makeIdentity();
    m.pos = m_pNode->getLocalToWorldMatrix().pos;
    for (int face = 0; face < NumClipDistances; ++face)
    {
        float distance = m_clipDistances[face];
        if (distance == 0.0f)
        {
            mask &= ~(1u << face);
            continue;
        }
        if (distance > m_range)
            distance = m_range;
        m.rotation() = faceRotations[face];
        // the apex and the four far corners of the face pyramid
        Vec3 points[8];
        computeFrustumPoints(points, HALF_PI, 0.0f, distance, m);
        for (int i = 0; i < 6; ++i)
        {
            const Plane& c = camera.getPlane(i);
            int behind = 0;
            for (int k = 3; k < 8; ++k)
                if (c.distance(points[k]) < 0.0f)
                    ++behind;
            if (behind == 5)
            {
                mask &= ~(1u << face);
                break;
            }
        }
    }
    return mask;
}
// 0x004b3490 (bounds) / 0x004b3490 (visualize)
AABox3 LightEntity::getWorldBounds() const
{
    if (m_lightType == Point)
    {
        const Vec3& pos = m_pNode->getLocalToWorldMatrix().pos;
        return AABox3(pos - Vec3(m_range, m_range, m_range), pos + Vec3(m_range, m_range, m_range));
    }
    if (m_lightType == Spot)
    {
        float halfExtent = (float)std::tan(m_spotAngle * 0.5f) * m_range;
        AABox3 local(Vec3(-halfExtent, -halfExtent, 0.0f), Vec3(halfExtent, halfExtent, m_range));
        return transformBox(local, m_pNode->getLocalToWorldMatrix());
    }
    return AABox3(Vec3(0, 0, 0), Vec3(0, 0, 0));
}
void LightEntity::visualize()
{
    const Matrix4x3& m = m_pNode->getLocalToWorldMatrix();
    if (m_lightType == Point)
    {
        DebugDraw::drawPointLight(m.pos, 1.0f, Color::Yellow);
        DebugDraw::drawSphere(m.pos, m_range, Color::Yellow, 6);
    }
    else if (m_lightType == Spot)
    {
        DebugDraw::drawPointLight(m.pos, 1.0f, Color::Yellow);
        DebugDraw::drawFrustum(m, m_spotAngle, 1.0f, 0.1f, m_range, Color::Yellow);
    }
}

// ---- OccluderEntity ------------------------------------------------------------

// 0x004b4d70
OccluderEntity::OccluderEntity(Mesh* mesh) : RenderEntity(OccluderEntityType), m_radius(0.0f)
{
    setMesh(mesh);
}
// 0x004b4e00
OccluderEntity::~OccluderEntity() {}
// 0x004b4af0
void OccluderEntity::setMesh(Mesh* mesh)
{
    m_mesh.reset(mesh);
    m_unbounded = m_mesh.get() == 0;
    notifyBoundChanged();
    m_radius = 0.0f;
    if (mesh)
    {
        const Vec3* positions =
            (const Vec3*)mesh->getVertexArray(Mesh::Position, Mesh::TypeFloat, 3);
        for (int i = 0; i < mesh->getNumVertices(); ++i)
        {
            float d = positions[i].x * positions[i].x + positions[i].y * positions[i].y +
                      positions[i].z * positions[i].z;
            if (d > m_radius)
                m_radius = d;
        }
        m_radius = sqrtf(m_radius);
    }
}
// 0x004b3800
AABox3 OccluderEntity::getWorldBounds() const
{
    return transformBox(m_mesh->getBoundingBox(), m_pNode->getLocalToWorldMatrix());
}

} // namespace engine
