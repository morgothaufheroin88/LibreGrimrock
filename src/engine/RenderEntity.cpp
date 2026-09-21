// Reconstructed from Grimrock.bin.x86 RenderEntity.cpp.
#include "engine/RenderEntity.h"
#include "core/DebugDraw.h"
#include "core/Math.h"
#include "engine/Mesh.h"
#include "engine/Renderer.h"
#include "engine/Scene.h"
#include "engine/SpatialDS.h"
#include "engine/Texture.h"
#include <cmath>

namespace engine
{

constexpr float DefaultMaxShadowDistance = 100000.0f;
constexpr int DefaultShadowMapSize = 256;

using namespace core;

// ---- Skeleton ------------------------------------------------------------------

// 0x080ece90
void Skeleton::addBone(Node& bone, const Matrix4x3& invBindMatrix)
{
    m_bones.push_back(&bone);
    m_invBindMatrices.push_back(invBindMatrix);
}
// 0x080ed0e0
void Skeleton::computeSkinningMatrices(const Matrix4x3& worldToModel, Matrix4x3* out) const
{
    for (int i = 0; i < m_bones.size(); ++i)
        out[i] = worldToModel * (m_bones[i]->getLocalToWorldMatrix() * m_invBindMatrices[i]);
}
// 0x080ecde0
void Skeleton::drawRestPose()
{
    for (int i = 0; i < m_bones.size(); ++i)
    {
        Matrix4x3 m = m_invBindMatrices[i];
        m.invert();
        DebugDraw::drawBase(m, 0.1f);
    }
}

// ---- RenderEntity --------------------------------------------------------------

// 0x080ec990
RenderEntity::RenderEntity(EntityType type)
    : Component(RenderEntityComponent), m_unbounded(false), m_pSpatialNode(0), m_entityType(type),
      m_drawBoundBox(false), m_hidden(false), m_sortOffset(0.0f)
{
}
// 0x080ec9d0
RenderEntity::~RenderEntity() {}
// 0x080ec9e0
void RenderEntity::addedToNode(Node* node)
{
    if (node->getScene())
        m_pSpatialNode = node->getScene()->getSpatialDS()->addEntity(*this);
}
// 0x080eca10
void RenderEntity::removedFromNode()
{
    if (m_pNode->getScene() && m_pSpatialNode)
        m_pNode->getScene()->getSpatialDS()->removeEntity(*m_pSpatialNode);
    m_pSpatialNode = 0;
}
// 0x080ee880
void RenderEntity::visualize() {}

// ---- MeshEntity ----------------------------------------------------------------

// 0x080ee470
MeshEntity::MeshEntity(RenderableMesh* mesh) : RenderEntity(MeshEntityType), m_flags(CastShadow)
{
    setMesh(mesh);
}
// 0x080ed9a0
MeshEntity::~MeshEntity() {}
// 0x080eddf0: adopts the mesh's default materials.
void MeshEntity::setMesh(RenderableMesh* mesh)
{
    m_mesh.reset(mesh);
    m_materials.clear();
    if (mesh)
        m_materials = mesh->getMaterials();
    m_unbounded = m_mesh.get() == 0;
    if (m_pSpatialNode)
        m_pSpatialNode->notifyBoundChanged();
}
// 0x080eddd0
void MeshEntity::setSkeleton(Skeleton* skeleton)
{
    m_skeleton.reset(skeleton);
}
// 0x080ed880: same material on every segment.
void MeshEntity::setMaterial(Material* material)
{
    for (int i = 0; i < m_materials.size(); ++i)
        m_materials[i].reset(material);
}
// 0x080ee570
void MeshEntity::setMaterials(const Array<SharedPtr<Material>>& materials)
{
    if (&m_materials != &materials)
        m_materials = materials;
}
// 0x080ecd40
AABox3 MeshEntity::getWorldBounds()
{
    if (m_flags & ManualBounds)
        return m_bounds;
    return transformBox(m_mesh->getBoundingBox(), m_pNode->getLocalToWorldMatrix());
}
// 0x080ed0a0
void MeshEntity::visualize()
{
    DebugDraw::drawBox(getWorldBounds(), Color::White);
}

// ---- LightEntity ---------------------------------------------------------------

// 0x080ed7d0
LightEntity::LightEntity(LightType type)
    : RenderEntity(LightEntityType), m_lightType(type), m_color(1, 1, 1), m_range(1.0f),
      m_spotAngle(HALF_PI), m_spotSharpness(0.0f), m_castShadow(false),
      m_maxShadowDistance(DefaultMaxShadowDistance), m_shadowMapSize(DefaultShadowMapSize),
      m_primaryLight(false)
{
    m_unbounded = type < Point;
}
// 0x080ed8c0
LightEntity::~LightEntity() {}
// 0x080eca50
void LightEntity::setLightType(LightType type)
{
    m_lightType = type;
    m_unbounded = type < Point;
    if (m_pSpatialNode)
        m_pSpatialNode->notifyBoundChanged();
}
// 0x080eca80
void LightEntity::setLightRange(float range)
{
    m_range = range;
    if (m_pSpatialNode)
        m_pSpatialNode->notifyBoundChanged();
}
// 0x080ecab0
void LightEntity::setSpotAngle(float angle)
{
    m_spotAngle = angle;
    if (m_pSpatialNode)
        m_pSpatialNode->notifyBoundChanged();
}
// 0x080ed860
void LightEntity::setStaticShadowMap(RenderableTexture* texture)
{
    m_staticShadowMap.reset(texture);
}
// 0x080ecae0
RenderableTexture* LightEntity::renderStaticShadowMap(int size, float bias, int flags)
{
    return Renderer::sm_pActiveRenderer->renderStaticShadowMap(*this, size, bias, flags);
}
// 0x080ecc40
AABox3 LightEntity::getWorldBounds()
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
// 0x080ecb50
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

} // namespace engine
