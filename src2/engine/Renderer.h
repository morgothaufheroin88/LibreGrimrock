// Renderer interface, visitors and post processing filters of Legend of Grimrock 2,
// reconstructed from grimrock2.exe Renderer.cpp (0x004a7320-0x004aaa20), the render
// visitor (0x004aaa20-0x004ab580) and FogFilter.cpp (0x004ab580-0x004ab700).
#pragma once
#include "core/Array.h"
#include "core/Color.h"
#include "core/Prim.h"
#include "core/SharedPtr.h"
#include "core/String.h"
#include "engine/Node.h"
#include <utility>

namespace core
{
class Window;
}

namespace engine
{

class Scene;
class Camera;
class LightEntity;
class MeshEntity;
class ParticleEntity;
class OccluderEntity;
class RenderEntity;
class RenderableMesh;
class RenderableTexture;
class RenderableShader;
class VPXPlayer;

enum RenderEngine
{
    RenderEngine_D3D9 = 0,
    RenderEngine_GLES2 = 1,
    RenderEngine_OpenGL = 2
};

// 0x004aa960
struct RendererConfig
{
    static constexpr int DefaultWidth = 1280;
    static constexpr int DefaultHeight = 720;
    int renderEngine;
    core::Window* window;
    int width;
    int height;
    int multisamples;
    bool windowed;
    int verticalSync; // 0 off, 1 on, 2 triple buffer
    core::String altShaderPath;
    bool notebookMode;
    RendererConfig()
        : renderEngine(RenderEngine_D3D9), window(0), width(DefaultWidth), height(DefaultHeight),
          multisamples(0), windowed(true), verticalSync(1), notebookMode(false)
    {
    }
};

struct RendererInfo
{
    core::String renderer;
    core::String vendor;
    core::String version;
    core::String shadingLanguage;
};

class RenderWindow
{
  public:
    virtual ~RenderWindow() {}
};

// Surface shader program sets: the light pre-pass renderer's five passes, the notebook
// renderer's lit/unlit pair, or a post process program (0x004c4f60).
class RenderableShader
{
  public:
    virtual ~RenderableShader() {}
    virtual void initLightPrePassRendererShader(const char* vertexShader,
                                                const char* geometryShader,
                                                const char* materialShader, const char* unlitShader,
                                                const char* shadowShader) = 0;
    virtual void initNotebookRendererShader(const char* vertexShader, const char* fragmentShader,
                                            const char* unlitShader) = 0;
    virtual void initPostProcessShader(const char* fragmentShader) = 0;
};

// 0x0061ccb8 (Renderer.getRenderStats, 0x0061a958 name table)
struct RenderStats
{
    int visibleMeshes;
    int visibleLights;
    int visibleParticleSystems;
    int visibleOccluders;
    int drawSegments;
    int shadowSegments;
    int renderTriangles;
    int bindShader;
    int bindMaterial;
};
extern RenderStats g_renderStats;

// Screen space ambient occlusion settings (Lua SSAOFilter).
struct SSAOFilter
{
    int quality;
    float intensity;
};

// Tone mapping settings (Lua Tonemapper).
struct Tonemapper
{
    float saturation;
};

// Fog settings and the fog particles (0x004ab580-0x004ab700; Lua FogFilter).
class FogFilter
{
  public:
    enum FogMode
    {
        Fog_Linear = 0,
        Fog_Exp = 1,
        Fog_LinearLit = 2,
        Fog_Dense = 3
    };
    // x, y, z, size, ? per particle (FogFilter.setParticles takes 5 numbers each)
    struct Particle
    {
        float x, y, z;
        float size;
        float phase;
    };
    FogFilter();
    virtual ~FogFilter();

    core::Array<Particle> m_particles;
    int m_fogMode;
    core::Vec3 m_fogColor;
    core::Vec2 m_fogRange;
    float m_fogDensity;
    core::Vec3 m_fogLightDirection;
    core::SharedPtr<RenderableTexture> m_particleTexture;
    float m_particleSize;
    core::Vec3 m_particleColor;
};

// Cinematic player (0x004f2550 vtable: open, close, update, isDone, getWidth, getHeight).
class VPXPlayer
{
  public:
    virtual ~VPXPlayer() {}
    virtual void open(const char* filename) = 0;
    virtual void close() = 0;
    virtual void update() = 0;
    virtual bool isDone() = 0;
    virtual int getWidth() = 0;
    virtual int getHeight() = 0;
};

class Renderer
{
  public:
    enum TextureFilter
    {
        TextureFilter_Bilinear = 0,
        TextureFilter_Trilinear = 1,
        TextureFilter_Anisotropic = 2
    };
    // Renderer.setFlag (0x0061a1a0)
    enum Flag
    {
        Flag_DrawNormalBuffer = 1,
        Flag_DrawGlossinessBuffer = 2,
        Flag_DrawLightBuffer = 4,
        Flag_DrawAmbientOcclusionBuffer = 8,
        Flag_DrawOcclusionCullingBuffer = 16,
        Flag_ForceNotebookMode = 32
    };

    // 0x004aa9d0
    Renderer();
    // 0x004aa860
    virtual ~Renderer();
    virtual void init(const RendererConfig& config) = 0;
    virtual RendererInfo getRendererInfo() = 0;
    virtual void resizeRenderBuffers(int width, int height) = 0;
    virtual bool isReadyToRender() = 0;
    // 0x004a7320: no-op in every back end
    virtual void setRenderWindow(RenderWindow* window) {}
    virtual void setRefractionBuffer(RenderableTexture* buffer) = 0;
    virtual void setRefractionClipPlane(const core::Vec4& plane) = 0;
    virtual void beginRender() = 0;
    virtual void renderScene(Scene& scene, Camera& camera, RenderableTexture* target,
                             int passMask) = 0;
    // extents receives how far the rendered geometry reached per cube face (6 floats).
    virtual RenderableTexture* renderStaticShadowMap(LightEntity& light, int size, float bias,
                                                     int flags, float* extents) = 0;
    virtual void endRender() = 0;
    virtual void saveScreenShot(const char* filename) {}
    virtual RenderableMesh* createRenderableMesh() = 0;
    virtual RenderableTexture* createRenderableTexture() = 0;
    virtual RenderableShader* createRenderableShader() = 0;
    virtual RenderWindow* createRenderWindow(const core::Window& window) = 0;
    // format: 0 rgba_8, 1 rgba_16f (0x0061a248)
    virtual RenderableTexture* createRenderBuffer(int width, int height, int format) = 0;
    virtual VPXPlayer* createVPXPlayer() = 0;
    virtual SSAOFilter* getSSAOFilter() = 0;
    virtual FogFilter* getFogFilter() = 0;
    virtual Tonemapper* getTonemapper() = 0;
    // 0x004aa880
    virtual RenderableTexture* getGeometryBuffer()
    {
        return 0;
    }
    virtual void enumerateResolutions(core::Array<std::pair<int, int>>& resolutions) = 0;
    virtual int getAvailableTextureMemory() = 0;

    // 0x004aa890
    static Renderer* create(int renderEngine);
    static Renderer* getActiveRenderer()
    {
        return sm_pActiveRenderer;
    }
    static Renderer* sm_pActiveRenderer;

    // Settings (offsets 0x04-0x30 in the original object).
    core::Color m_clearColor;
    bool m_wireframe;
    bool m_ambientOcclusion;
    bool m_fog;
    bool m_diffuseMapping;
    bool m_normalMapping;
    int m_textureFilter;
    bool m_fxaa;
    bool m_occlusionCulling;
    bool m_renderMeshes;
    bool m_renderShadows;
    int m_shadowQuality;
    int m_occlusionCullingResolutionDivider;
    unsigned int m_flags;
    int m_viewportX, m_viewportY, m_viewportWidth, m_viewportHeight;
};

// Collects the visible entities of a scene for one camera (0x004aaa20-0x004ab580).
class RenderVisitor
{
  public:
    static constexpr int ReserveEntities = 0x800;
    static constexpr int MaxPlanes = 12; // frustum plus the camera's user clip planes

    // 0x004ab610
    RenderVisitor();
    ~RenderVisitor();
    // 0x004aaa20: entities inside the camera frustum whose pass mask matches, sorted by
    // type; skinning and shadow distances of the meshes are evaluated here.
    void gatherEntities(Scene& scene, Camera& camera, unsigned int passMask);
    // 0x004ab340: shadow casters inside the given planes.
    void gatherShadowCasters(Scene& scene, const Camera& camera, const core::Plane* planes,
                             int numPlanes);

    core::Array<RenderEntity*> m_entities;
    core::Array<MeshEntity*> m_meshes;
    core::Array<LightEntity*> m_lights;
    core::Array<ParticleEntity*> m_particles;
    core::Array<OccluderEntity*> m_occluders;
};

} // namespace engine
