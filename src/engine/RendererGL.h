// OpenGL renderer and resources, reconstructed from RendererGL.cpp (0x081100c0-0x08114b80),
// CommonResourcesGL.cpp and RenderContextSDL.cpp.
#pragma once
#include "engine/Material.h"
#include "engine/Mesh.h"
#include "engine/RenderContextGL.h"
#include "engine/Renderer.h"
#include "engine/Texture.h"
#include <SDL3/SDL.h>

namespace core
{
class Window;
}

namespace engine
{

class Font;
class LightPrePassRendererGL;
class ScreenSpaceAmbientOcclusionGL;
class FogFilterGL;
class TonemapperGL;
class NotebookRendererGL;
class ImmediateModeGL;
class GraphicsGL;

// SDL2 owned GL context (0x0810fd30-0x08110040).
class RenderContextSDL : public RenderContextGL
{
  public:
    RenderContextSDL(core::Window* window, bool windowed, bool verticalSync);
    ~RenderContextSDL();
    void swapBuffers();
    static void SetupGLAttributes();
    static void enumerateResolutions(core::Array<std::pair<int, int>>& resolutions);

  private:
    // When the window is larger than the requested frame (fullscreen at the desktop
    // resolution), the frame is rendered into this off-screen back buffer of the requested
    // size and scaled onto the window at swap time.
    void createBackBuffer(int width, int height);
    void destroyBackBuffer();

  public:
    // Creates or drops the back buffer when the window size differs from the frame size
    // (fullscreen windows take the desktop size after creation).
    void updateBackBuffer();

  private:
    core::Window* m_pCoreWindow;
    SDL_Window* m_pWindow;
    SDL_GLContext m_context;
    GLuint m_backBuffer;
    GLuint m_backBufferColor;
    GLuint m_backBufferDepth;
    int m_backBufferWidth;
    int m_backBufferHeight;
};

class RenderWindowGL : public RenderWindow
{
  public:
    ~RenderWindowGL() {}
};

class RenderableTextureGL : public RenderableTexture
{
  public:
    RenderableTextureGL();
    ~RenderableTextureGL();
    void init(const core::Image& image);
    // 0x08112760: DDS through the texture asset processor; skipMipLevels drops the
    // largest levels, srgb selects the sRGB internal formats.
    void load(const char* filename, int skipMipLevels, bool srgb);
    void reload();
    void save(const char* filename);
    int getWidth() const;
    int getHeight() const;
    TextureGL* getTexture() const
    {
        return m_pTexture;
    }
    void setTexture(TextureGL* texture);

  private:
    TextureGL* m_pTexture;
};

// Compiles the surface shader variants: [pass][skinning | normalMap << 1 | alphaTest << 2].
class RenderableShaderGL : public RenderableShader
{
  public:
    enum Pass
    {
        GeometryPass = 0,
        MaterialPass = 1,
        UnlitPass = 2,
        TransparentPass = 3,
        ShadowPass = 4,
        NumPasses = 5
    };
    enum
    {
        Skinning = 1,
        NormalMap = 2,
        AlphaTest = 4,
        NumVariants = 8
    };
    RenderableShaderGL();
    ~RenderableShaderGL();
    void initSurfaceShader(const char* vertexShader, const char* geometryShader,
                           const char* materialShader, const char* unlitShader,
                           const char* shadowShader);
    void initPostProcessShader(const char* fragmentShader);
    ShaderProgramGL* getProgram(int pass, int variant) const
    {
        return m_programs[pass][variant];
    }
    ShaderProgramGL* getPostProcessProgram() const
    {
        return m_programs[0][0];
    }

  private:
    ShaderProgramGL* m_programs[NumPasses][NumVariants];
};

class RenderableMeshGL : public RenderableMesh
{
  public:
    RenderableMeshGL();
    ~RenderableMeshGL();
    void init(Mesh& mesh, bool keepSourceData);
    Mesh* getSourceData();
    const core::AABox3& getBoundingBox() const;
    core::Array<core::SharedPtr<Material>> getMaterials() const;
    void activate();
    void renderSegment(int segment);
    void drawNormals(const core::Matrix4x3& m);
    void drawTangents(const core::Matrix4x3& m);
    int getNumSegments() const
    {
        return m_segments.size();
    }
    const MeshSegment& getSegment(int i) const
    {
        return m_segments[i];
    }
    bool isSkinned() const
    {
        return m_skinned;
    }
    bool hasTangents() const
    {
        return m_tangentOffset >= 0;
    }

  private:
    core::SharedPtr<Mesh> m_sourceData;
    GLuint m_vertexBuffer;
    GLuint m_indexBuffer;
    int m_indexSize; // 2 or 4 bytes
    core::Array<MeshSegment> m_segments;
    int m_numVertices;
    int m_stride;
    int m_normalOffset, m_tangentOffset, m_bitangentOffset, m_texcoordOffset;
    int m_boneIndicesOffset, m_boneWeightsOffset;
    bool m_skinned;
    core::AABox3 m_bounds;
};

// Shared textures, meshes, shaders and fonts (0x08114f10-0x081154e0).
class CommonResourcesGL
{
  public:
    CommonResourcesGL();
    ~CommonResourcesGL();
    static RenderableTextureGL* WhiteMap;
    static RenderableTextureGL* DefaultNormalMap;
    static RenderableTextureGL* RotMap;
    static RenderableMeshGL* SphereMesh;
    static RenderableMeshGL* ConeMesh;
    static Font* DefaultFont;
    static RenderableShaderGL* BlitShader;
    static Material* BlitMaterial;

  private:
    core::Array<core::SharedPtr<RenderableTextureGL>> m_textures;
    core::Array<core::SharedPtr<RenderableMeshGL>> m_meshes;
    core::Array<core::SharedPtr<RenderableShader>> m_shaders;
    core::Array<core::SharedPtr<Material>> m_materials;
    core::Array<core::SharedPtr<Font>> m_fonts;
};

class RendererGL : public Renderer
{
  public:
    RendererGL();
    ~RendererGL();
    void init(const RendererConfig& config);
    RendererInfo getRendererInfo();
    void resizeRenderBuffers(int width, int height);
    bool isReadyToRender();
    void setRenderWindow(RenderWindow* window);
    void beginRender();
    void renderScene(Scene& scene, Camera& camera, RenderableTexture* target);
    RenderableTexture* renderStaticShadowMap(LightEntity& light, int size, float bias, int flags);
    void endRender();
    void saveScreenShot(const char* filename);
    RenderableMesh* createRenderableMesh();
    RenderableTexture* createRenderableTexture();
    RenderableShader* createRenderableShader();
    RenderWindow* createRenderWindow(const core::Window& window);
    RenderableTexture* createRenderBuffer(int width, int height);
    void enumerateResolutions(core::Array<std::pair<int, int>>& resolutions);
    int getAvailableTextureMemory();

    RenderContextSDL* getContext() const
    {
        return m_pContext;
    }
    const RendererConfig& getConfig() const
    {
        return m_config;
    }

  private:
    // 0x08110660: fixed function fullscreen quad of a buffer for debugging.
    void drawBuffer(Texture2DGL* texture, float scale);

    RendererConfig m_config;
    RenderContextSDL* m_pContext;
    GraphicsGL* m_pGraphics;
    CommonResourcesGL* m_pCommonResources;
    RenderVisitor* m_pRenderVisitor;
    ImmediateModeGL* m_pImmediateMode;
    NotebookRendererGL* m_pNotebookRenderer;
    LightPrePassRendererGL* m_pLightPrePassRenderer;
    ScreenSpaceAmbientOcclusionGL* m_pAmbientOcclusion;
    FogFilterGL* m_pFogFilter;
    TonemapperGL* m_pTonemapper;
};

} // namespace engine
