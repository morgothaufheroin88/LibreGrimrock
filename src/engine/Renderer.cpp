// Reconstructed from Grimrock.bin.x86 Renderer.cpp and RenderBuffer.cpp.
#include "engine/Renderer.h"
#include "core/DebugDraw.h"
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
RenderStats g_renderStats = {0, 0, 0, 0, 0};
Array<SharedPtr<RenderableTexture>> RenderBuffer::sm_freeTemps;
Array<SharedPtr<RenderableTexture>> RenderBuffer::sm_allocatedTemps;

// 0x080f1240
Renderer::Renderer()
    : m_clearColor(50, 50, 50, 255), m_wireframe(false), m_ambientOcclusion(false),
      m_ssaoQuality(1), m_fog(false), m_fogColor(0, 0, 0), m_fogStart(0.0f), m_fogEnd(0.0f),
      m_diffuseMapping(true), m_normalMapping(true), m_textureFilter(TextureFilter_Anisotropic),
      m_fxaa(false), m_renderMeshes(true), m_renderShadows(true), m_shadowQuality(1),
      m_drawNormalBuffer(false), m_drawGlossinessBuffer(false), m_drawLightBuffer(false),
      m_drawAmbientOcclusionBuffer(false), m_drawStats(false), m_viewportX(0), m_viewportY(0),
      m_viewportWidth(0), m_viewportHeight(0)
{
}
// 0x080f12c0
Renderer::~Renderer()
{
    if (sm_pActiveRenderer == this)
        sm_pActiveRenderer = 0;
}
// 0x080f15c0: only the OpenGL back end exists in the Linux build.
Renderer* Renderer::create(int renderEngine)
{
    if (renderEngine != RenderEngine_OpenGL)
        return 0;
    RendererGL* renderer = new RendererGL;
    sm_pActiveRenderer = renderer;
    return renderer;
}

// 0x080f1e30
void drawRenderStats()
{
    constexpr float LineHeight = 12.0f;
    Vec2 pos(50.0f, 120.0f);
    DebugDraw::drawText(formatString("Visible meshes:   %d", g_renderStats.visibleMeshes).c_str(),
                        pos, Color::White);
    pos.y += LineHeight;
    DebugDraw::drawText(formatString("Visible lights:   %d", g_renderStats.visibleLights).c_str(),
                        pos, Color::White);
    pos.y += LineHeight;
    DebugDraw::drawText(formatString("Draw segments:    %d", g_renderStats.drawSegments).c_str(),
                        pos, Color::White);
    pos.y += LineHeight;
    DebugDraw::drawText(formatString("Shadow segments:  %d", g_renderStats.shadowSegments).c_str(),
                        pos, Color::White);
}

// ---- RenderVisitor ---------------------------------------------------------------

// 0x080f1a00
RenderVisitor::RenderVisitor()
{
    m_meshes.reserve(1024);
    m_lights.reserve(1024);
    m_particles.reserve(1024);
}
RenderVisitor::~RenderVisitor() {}
// 0x080f1340
void RenderVisitor::visit(Node* node)
{
    RenderEntity* entity = node->getRenderEntity();
    if (!entity || entity->getHidden())
        return;
    switch (entity->getEntityType())
    {
    case RenderEntity::LightEntityType:
        m_lights.push_back((LightEntity*)entity);
        break;
    case RenderEntity::ParticleEntityType:
        if (((ParticleEntity*)entity)->getParticleSystem())
            m_particles.push_back((ParticleEntity*)entity);
        break;
    case RenderEntity::MeshEntityType:
        if (((MeshEntity*)entity)->getMesh())
            m_meshes.push_back((MeshEntity*)entity);
        break;
    }
}
// 0x080f1620
void RenderVisitor::gatherEntities(Scene& scene, Camera& camera)
{
    Profiler::beginBlock("gatherEntities");
    Plane planes[6];
    for (int i = 0; i < 6; ++i)
        planes[i] = camera.getPlane(i);
    m_meshes.resize(0);
    m_lights.resize(0);
    m_particles.resize(0);
    scene.query(planes, 6, *this);
    g_renderStats.visibleMeshes = m_meshes.size();
    g_renderStats.visibleLights = m_lights.size();
    g_renderStats.visibleParticles = m_particles.size();
    Profiler::endBlock();
}

// ---- ShadowMapVisitor ------------------------------------------------------------

// 0x080f1ff0
ShadowMapVisitor::ShadowMapVisitor()
{
    m_meshes.reserve(1024);
}
ShadowMapVisitor::~ShadowMapVisitor() {}
// 0x080f1d50
void ShadowMapVisitor::visit(Node* node)
{
    MeshEntity* entity = node->getMeshEntity();
    if (entity && entity->getMesh() && entity->getFlag(MeshEntity::CastShadow) &&
        !entity->getHidden())
        m_meshes.push_back(entity);
}
// 0x080f1c70
void ShadowMapVisitor::gatherEntities(Scene& scene, Plane* planes, int numPlanes)
{
    m_meshes.resize(0);
    scene.query(planes, numPlanes, *this);
}

// ---- RenderBuffer ----------------------------------------------------------------

// 0x080f2840: reuse a free buffer of the same size, else create one.
RenderableTexture* RenderBuffer::getTemporary(int width, int height)
{
    for (int i = 0; i < sm_freeTemps.size(); ++i)
    {
        if (sm_freeTemps[i]->getWidth() == width && sm_freeTemps[i]->getHeight() == height)
        {
            SharedPtr<RenderableTexture> buffer = sm_freeTemps[i];
            sm_freeTemps.erase(i);
            sm_allocatedTemps.push_back(buffer);
            return buffer.get();
        }
    }
    SharedPtr<RenderableTexture> buffer(
        Renderer::sm_pActiveRenderer->createRenderBuffer(width, height));
    sm_allocatedTemps.push_back(buffer);
    return buffer.get();
}
// 0x080f2fe0
void RenderBuffer::release(RenderableTexture* buffer)
{
    SharedPtr<RenderableTexture> ref(buffer);
    for (int i = 0; i < sm_allocatedTemps.size(); ++i)
    {
        if (sm_allocatedTemps[i] == ref)
        {
            sm_allocatedTemps.erase(i);
            sm_freeTemps.push_back(ref);
            return;
        }
    }
}
// 0x080f22f0
void RenderBuffer::releaseAllTemporaries()
{
    for (int i = 0; i < sm_allocatedTemps.size(); ++i)
        sm_freeTemps.push_back(sm_allocatedTemps[i]);
    sm_allocatedTemps.resize(0);
}
// 0x080f33f0
void RenderBuffer::freeAllTemporaries()
{
    sm_allocatedTemps.resize(0);
    sm_freeTemps.resize(0);
}

} // namespace engine
