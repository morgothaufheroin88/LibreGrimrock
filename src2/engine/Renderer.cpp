// Reconstructed from grimrock2.exe Renderer.cpp and FogFilter.cpp.
#include "engine/Renderer.h"
#include "core/Profiler.h"
#include "engine/Camera.h"
#include "engine/ParticleSystem.h"
#include "engine/RenderEntity.h"
#include "engine/RendererGL.h"
#include "engine/Scene.h"
#include "engine/Texture.h"

namespace engine
{

using namespace core;

Renderer* Renderer::sm_pActiveRenderer = 0;
RenderStats g_renderStats = {0, 0, 0, 0, 0, 0, 0, 0, 0};

// 0x004aa9d0
Renderer::Renderer()
    : m_clearColor(50, 50, 50, 255), m_wireframe(false), m_ambientOcclusion(false), m_fog(false),
      m_diffuseMapping(true), m_normalMapping(true), m_textureFilter(TextureFilter_Anisotropic),
      m_fxaa(false), m_occlusionCulling(true), m_renderMeshes(true), m_renderShadows(true),
      m_shadowQuality(1), m_occlusionCullingResolutionDivider(3), m_flags(0), m_viewportX(0),
      m_viewportY(0), m_viewportWidth(0), m_viewportHeight(0)
{
}
// 0x004aa860
Renderer::~Renderer()
{
    sm_pActiveRenderer = 0;
}
// 0x004aa890: the D3D9 back end of the original does not exist here.
Renderer* Renderer::create(int renderEngine)
{
    if (renderEngine != RenderEngine_OpenGL && renderEngine != RenderEngine_D3D9)
        return 0;
    RendererGL* renderer = new RendererGL;
    sm_pActiveRenderer = renderer;
    return renderer;
}

// ---- FogFilter -----------------------------------------------------------------

FogFilter::FogFilter()
    : m_fogMode(Fog_Linear), m_fogColor(0, 0, 0), m_fogRange(0, 0), m_fogDensity(0.0f),
      m_fogLightDirection(0, 0, 0), m_particleSize(0.0f), m_particleColor(0, 0, 0)
{
}
// 0x004ab6d0
FogFilter::~FogFilter() {}

// ---- RenderVisitor -------------------------------------------------------------

// 0x004ab610
RenderVisitor::RenderVisitor()
{
    m_entities.reserve(ReserveEntities);
    m_meshes.reserve(ReserveEntities);
    m_lights.reserve(ReserveEntities);
    m_particles.reserve(ReserveEntities);
    m_occluders.reserve(ReserveEntities);
}
RenderVisitor::~RenderVisitor() {}

// 0x004aaa20
void RenderVisitor::gatherEntities(Scene& scene, Camera& camera, unsigned int passMask)
{
    ProfileScope profile("_GatherEntities");
    Plane planes[MaxPlanes];
    int numPlanes = 6;
    for (int i = 0; i < 6; ++i)
        planes[i] = camera.getPlane(i);
    for (int i = 0; i < Camera::NumUserClipPlanes; ++i)
    {
        if (camera.getUserClipPlaneMask() & (1u << i))
        {
            // the user planes are stored with the opposite sign convention
            const Plane& p = camera.getUserClipPlane(i);
            planes[numPlanes++] = Plane(p.normal, -p.d);
        }
    }
    m_entities.clear();
    m_meshes.clear();
    m_lights.clear();
    m_particles.clear();
    m_occluders.clear();
    scene.query(planes, numPlanes, m_entities);
    const Vec3& eye = camera.getLocalToWorldMatrix().pos;
    float invLodFactor = 1.0f / camera.getLodFactor();
    for (int i = 0; i < m_entities.size(); ++i)
    {
        RenderEntity* entity = m_entities[i];
        if (entity->getHidden() || (entity->getPassMask() & passMask) == 0)
            continue;
        switch (entity->getEntityType())
        {
        case RenderEntity::MeshEntityType:
        {
            MeshEntity* mesh = (MeshEntity*)entity;
            if (!mesh->getMesh())
                break;
            Vec3 d = mesh->getNode()->getLocalToWorldMatrix().pos - eye;
            float distance = (d.x * d.x + d.y * d.y + d.z * d.z) * invLodFactor;
            mesh->setDistance(distance);
            // skinning stops beyond the skinning distance
            mesh->setSkinned(mesh->getSkeleton() != 0 &&
                             distance < mesh->getSkinningDistance() * mesh->getSkinningDistance());
            // a mesh dissolving with distance is skipped once it has faded out
            float start = mesh->getDissolveStart(), end = mesh->getDissolveEnd();
            if (end == 0.0f || (start < end && distance < end * end) ||
                (end < start && end * end < distance))
                m_meshes.push_back(mesh);
            break;
        }
        case RenderEntity::LightEntityType:
        {
            LightEntity* light = (LightEntity*)entity;
            m_lights.push_back(light);
            if (light->getDebugDraw())
                light->visualize();
            break;
        }
        case RenderEntity::ParticleEntityType:
        {
            ParticleEntity* particles = (ParticleEntity*)entity;
            if (particles->getParticleSystem() && particles->getOpacity() > 0.0f)
                m_particles.push_back(particles);
            break;
        }
        case RenderEntity::OccluderEntityType:
            if (((OccluderEntity*)entity)->getMesh())
                m_occluders.push_back((OccluderEntity*)entity);
            break;
        }
        if (entity->getDrawBoundBox() && !entity->isUnbounded())
            entity->visualize();
    }
    g_renderStats.visibleMeshes = m_meshes.size();
    g_renderStats.visibleLights = m_lights.size();
    g_renderStats.visibleParticleSystems = m_particles.size();
    g_renderStats.visibleOccluders = m_occluders.size();
}

// 0x004ab340: shadow casting meshes inside the planes whose shadow distance range
// contains their distance to the camera.
void RenderVisitor::gatherShadowCasters(Scene& scene, const Camera& camera, const Plane* planes,
                                        int numPlanes)
{
    ProfileScope profile("_GatherEntities");
    m_entities.clear();
    m_meshes.clear();
    scene.query(planes, numPlanes, m_entities);
    const Vec3& eye = camera.getLocalToWorldMatrix().pos;
    float invLodFactor = 1.0f / camera.getLodFactor();
    for (int i = 0; i < m_entities.size(); ++i)
    {
        RenderEntity* entity = m_entities[i];
        if (entity->getEntityType() != RenderEntity::MeshEntityType || entity->getHidden())
            continue;
        MeshEntity* mesh = (MeshEntity*)entity;
        if (!mesh->getMesh() || !mesh->getFlag(MeshEntity::CastShadow))
            continue;
        Vec3 d = mesh->getNode()->getLocalToWorldMatrix().pos - eye;
        float distance = (d.x * d.x + d.y * d.y + d.z * d.z) * invLodFactor;
        mesh->setDistance(distance);
        mesh->setSkinned(mesh->getSkeleton() != 0 &&
                         distance < mesh->getSkinningDistance() * mesh->getSkinningDistance());
        if (distance >= mesh->getShadowMinDistance() * mesh->getShadowMinDistance() &&
            distance < mesh->getShadowMaxDistance() * mesh->getShadowMaxDistance())
            m_meshes.push_back(mesh);
    }
}

} // namespace engine
