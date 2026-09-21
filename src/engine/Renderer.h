// Renderer interface, visitors and temporary render buffers, reconstructed from
// Renderer.cpp (0x080f11f0-0x080f21b0) and RenderBuffer.cpp (0x080f22f0-0x080f33f0).
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
class RenderableMesh;
class RenderableTexture;
class RenderableShader;

enum RenderEngine
{
    RenderEngine_D3D9 = 0,
    RenderEngine_GLES2 = 1,
    RenderEngine_OpenGL = 2
};

// 0x080f11f0
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
        : renderEngine(RenderEngine_OpenGL), window(0), width(DefaultWidth), height(DefaultHeight),
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

// 0x08114650: surface (5 pass) or post process shader program set.
class RenderableShader
{
  public:
    virtual ~RenderableShader() {}
    virtual void initSurfaceShader(const char* vertexShader, const char* geometryShader,
                                   const char* materialShader, const char* unlitShader,
                                   const char* shadowShader) = 0;
    virtual void initPostProcessShader(const char* fragmentShader) = 0;
};

struct RenderStats
{
    int visibleMeshes;
    int visibleLights;
    int drawSegments;
    int shadowSegments;
    int visibleParticles;
};
extern RenderStats g_renderStats;
void drawRenderStats();

class Renderer
{
  public:
    enum TextureFilter
    {
        TextureFilter_Bilinear = 0,
        TextureFilter_Trilinear = 1,
        TextureFilter_Anisotropic = 2
    };

    Renderer();
    virtual ~Renderer();
    virtual void init(const RendererConfig& config) = 0;
    virtual RendererInfo getRendererInfo() = 0;
    virtual void resizeRenderBuffers(int width, int height) = 0;
    virtual bool isReadyToRender() = 0;
    virtual void setRenderWindow(RenderWindow* window) = 0;
    virtual void beginRender() = 0;
    virtual void renderScene(Scene& scene, Camera& camera, RenderableTexture* target) = 0;
    virtual RenderableTexture* renderStaticShadowMap(LightEntity& light, int size, float bias,
                                                     int flags) = 0;
    virtual void endRender() = 0;
    virtual void saveScreenShot(const char* filename) = 0;
    virtual RenderableMesh* createRenderableMesh() = 0;
    virtual RenderableTexture* createRenderableTexture() = 0;
    virtual RenderableShader* createRenderableShader() = 0;
    virtual RenderWindow* createRenderWindow(const core::Window& window) = 0;
    virtual RenderableTexture* createRenderBuffer(int width, int height) = 0;
    virtual void enumerateResolutions(core::Array<std::pair<int, int>>& resolutions) = 0;
    virtual int getAvailableTextureMemory() = 0;

    // 0x080f15c0
    static Renderer* create(int renderEngine);
    static Renderer* getActiveRenderer()
    {
        return sm_pActiveRenderer;
    }
    static Renderer* sm_pActiveRenderer;

    // Settings (offsets 0x04-0x4c in the original object).
    core::Color m_clearColor;
    bool m_wireframe;
    bool m_ambientOcclusion;
    int m_ssaoQuality;
    bool m_fog;
    core::Vec3 m_fogColor;
    float m_fogStart;
    float m_fogEnd;
    bool m_diffuseMapping;
    bool m_normalMapping;
    int m_textureFilter;
    bool m_fxaa;
    bool m_renderMeshes;
    bool m_renderShadows;
    int m_shadowQuality;
    bool m_drawNormalBuffer;
    bool m_drawGlossinessBuffer;
    bool m_drawLightBuffer;
    bool m_drawAmbientOcclusionBuffer;
    bool m_drawStats;
    int m_viewportX, m_viewportY, m_viewportWidth, m_viewportHeight;
};

// Collects the visible entities of a scene for one camera (0x080f1340-0x080f1a00).
class RenderVisitor : public NodeVisitor
{
  public:
    RenderVisitor();
    ~RenderVisitor();
    void visit(Node* node);
    void gatherEntities(Scene& scene, Camera& camera);
    core::Array<MeshEntity*> m_meshes;
    core::Array<LightEntity*> m_lights;
    core::Array<ParticleEntity*> m_particles;
};

// Collects shadow casting meshes inside a set of planes (0x080f1c70-0x080f2160).
class ShadowMapVisitor : public NodeVisitor
{
  public:
    ShadowMapVisitor();
    ~ShadowMapVisitor();
    void visit(Node* node);
    void gatherEntities(Scene& scene, core::Plane* planes, int numPlanes);
    core::Array<MeshEntity*> m_meshes;
};

// Pool of temporary render targets shared by the post processing passes.
class RenderBuffer
{
  public:
    static RenderableTexture* getTemporary(int width, int height);
    static void release(RenderableTexture* buffer);
    static void releaseAllTemporaries();
    static void freeAllTemporaries();

  private:
    static core::Array<core::SharedPtr<RenderableTexture>> sm_freeTemps;
    static core::Array<core::SharedPtr<RenderableTexture>> sm_allocatedTemps;
};

} // namespace engine
