// OpenGL renderer and resources of Legend of Grimrock 2, reconstructed from grimrock2.exe
// RendererGL.cpp (0x004c4140-0x004c6300), RenderableMeshGL/TextureGL (0x004e2690-0x004e3ed0)
// and CommonResourcesGL (0x004e4460).
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
class WaterRefractionGL;
class NotebookRendererGL;
class ImmediateModeGL;
class GraphicsGL;
class OcclusionCullingGL;
class VPXPlayer;

// SDL owned GL context; the WGL context of the original (0x004e0cc0-0x004e1150).
class RenderContextSDL : public RenderContextGL
{
  public:
    RenderContextSDL(core::Window* window, bool windowed, bool verticalSync);
    ~RenderContextSDL();
    void swapBuffers();
    static void SetupGLAttributes();
    // 0x004e11f0
    static void enumerateResolutions(core::Array<std::pair<int, int>>& resolutions);

  private:
    // The frame is rendered into this off-screen back buffer of the requested size and
    // scaled (letterboxed) onto the window at swap time; see updateBackBuffer().
    void createBackBuffer(int width, int height);
    void destroyBackBuffer();

  public:
    // (Re)creates the back buffer for the requested frame size.
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
    // 0x004e35f0: BGRA image with generated mip maps
    void init(const core::Image& image);
    // 0x004e36f0: DDS through the texture asset processor; skipMipLevels drops the
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

// The surface shader program variants, indexed by a bit set of the pass and the vertex
// format (0x004f25c0-0x004f3710).
class RenderableShaderGL : public RenderableShader
{
  public:
    enum Variant
    {
        Skinning = 1,
        NormalMap = 2,
        AlphaTest = 4,
        MaterialPass = 8,
        UnlitPass = 16,
        ShadowPass = 32,
        ShadowDirLight = 64,
        NotebookPass = 128,
        NumPrograms = 150
    };
    RenderableShaderGL();
    ~RenderableShaderGL();
    // 0x004f25c0
    void initLightPrePassRendererShader(const char* vertexShader, const char* geometryShader,
                                        const char* materialShader, const char* unlitShader,
                                        const char* shadowShader);
    // 0x004f3110
    void initNotebookRendererShader(const char* vertexShader, const char* fragmentShader,
                                    const char* unlitShader);
    // 0x004f3710
    void initPostProcessShader(const char* fragmentShader);
    ShaderProgramGL* getProgram(int variant) const
    {
        return m_programs[variant];
    }
    ShaderProgramGL* getPostProcessProgram() const
    {
        return m_programs[0];
    }

  private:
    void setProgram(int variant, ShaderProgramGL* program);
    ShaderProgramGL* m_programs[NumPrograms];
};

class RenderableMeshGL : public RenderableMesh
{
  public:
    // 0x14 bytes: material, primitive mode, first index, primitive count
    struct Segment
    {
        core::SharedPtr<Material> material;
        GLenum mode;
        int firstIndex;
        int primitiveCount;
    };
    RenderableMeshGL();
    ~RenderableMeshGL();
    // 0x004e2790: packed vertex format in a vertex array object: position float3, normal
    // and tangent as normalised bytes, texcoord as scaled shorts, colour bytes, four bone
    // indices/weights.
    void init(Mesh& mesh, bool keepSourceData);
    Mesh* getSourceData();
    const core::AABox3& getBoundingBox() const;
    core::Array<core::SharedPtr<Material>> getMaterials() const;
    void drawNormals(const core::Matrix4x3& m);
    void drawTangents(const core::Matrix4x3& m);
    int getNumSegments() const
    {
        return m_segments.size();
    }
    const Segment& getSegment(int i) const
    {
        return m_segments[i];
    }
    GLuint getVertexArray() const
    {
        return m_vertexArray;
    }
    int getIndexSize() const
    {
        return m_indexSize;
    }
    int getNumIndices() const
    {
        return m_numIndices;
    }
    bool isSkinned() const
    {
        return m_skinned;
    }
    bool hasTangents() const
    {
        return m_tangentOffset >= 0;
    }
    // g_texcoordScaleOffset of the packed texture coordinates
    core::Vec4 getTexcoordScaleOffset() const
    {
        return core::Vec4(m_texcoordScale.x, m_texcoordScale.y, m_texcoordOffset.x,
                          m_texcoordOffset.y);
    }

  private:
    core::SharedPtr<Mesh> m_sourceData;
    GLuint m_vertexBuffer;
    GLuint m_indexBuffer;
    GLuint m_vertexArray;
    int m_numIndices;
    int m_indexSize; // 2 or 4 bytes
    core::Array<Segment> m_segments;
    int m_numVertices;
    int m_stride;
    int m_normalOffset, m_tangentOffset, m_texcoordOffset_, m_colorOffset;
    int m_boneIndicesOffset, m_boneWeightsOffset;
    bool m_skinned;
    core::Vec2 m_texcoordScale;
    core::Vec2 m_texcoordOffset;
    core::AABox3 m_bounds;
    // debug lines of the normals/tangents, built on demand
    GLuint m_debugVertexBuffer;
    GLuint m_debugIndexBuffer;
    GLuint m_debugVertexArray;
    int m_debugLineCount;
    int m_debugLineKind;
};

// Shared textures, meshes, the default material and font (0x004e4460-0x004e5380).
class CommonResourcesGL
{
  public:
    CommonResourcesGL();
    ~CommonResourcesGL();
    static RenderableTextureGL* BlackMap;
    static RenderableTextureGL* GrayMap;
    static RenderableTextureGL* WhiteMap;
    static RenderableTextureGL* DefaultNormalMap;
    static RenderableTextureGL* RotMap;
    static RenderableTextureGL* NoiseMap;
    static RenderableMeshGL* SphereMesh;
    static RenderableMeshGL* ConeMesh;
    static Material* DefaultMaterial;
    static Font* DefaultFont;

  private:
    core::Array<core::SharedPtr<RenderableTextureGL>> m_textures;
    core::Array<core::SharedPtr<RenderableMeshGL>> m_meshes;
    core::Array<core::SharedPtr<Material>> m_materials;
    core::Array<core::SharedPtr<Font>> m_fonts;
};

class RendererGL : public Renderer
{
  public:
    // Renderer.renderScene pass mask bits
    enum PassBits
    {
        Pass_Opaque = 1,
        Pass_Transparent1 = 4,
        Pass_Transparent2 = 5,
        Pass_Transparent3 = 6
    };

    // 0x004c5ab0
    RendererGL();
    // 0x004c5b70
    ~RendererGL();
    void init(const RendererConfig& config);
    RendererInfo getRendererInfo();
    void resizeRenderBuffers(int width, int height);
    bool isReadyToRender();
    void setRefractionBuffer(RenderableTexture* buffer);
    void setRefractionClipPlane(const core::Vec4& plane);
    void beginRender();
    void renderScene(Scene& scene, Camera& camera, RenderableTexture* target, int passMask);
    RenderableTexture* renderStaticShadowMap(LightEntity& light, int size, float bias, int flags,
                                             float* extents);
    void endRender();
    void saveScreenShot(const char* filename);
    RenderableMesh* createRenderableMesh();
    RenderableTexture* createRenderableTexture();
    RenderableShader* createRenderableShader();
    RenderWindow* createRenderWindow(const core::Window& window);
    RenderableTexture* createRenderBuffer(int width, int height, int format);
    VPXPlayer* createVPXPlayer();
    SSAOFilter* getSSAOFilter();
    FogFilter* getFogFilter();
    Tonemapper* getTonemapper();
    RenderableTexture* getGeometryBuffer();
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
    bool isNotebookMode() const
    {
        return m_config.notebookMode || (m_flags & Flag_ForceNotebookMode) != 0;
    }

  private:
    // 0x004c4990: fullscreen quad of a buffer scaled by a factor for debugging.
    void drawBuffer(TextureGL* texture, float scale);

    RendererConfig m_config;
    RenderContextSDL* m_pContext;
    GraphicsGL* m_pGraphics;
    CommonResourcesGL* m_pCommonResources;
    RenderVisitor* m_pRenderVisitor;
    ImmediateModeGL* m_pImmediateMode;
    OcclusionCullingGL* m_pOcclusionCulling;
    NotebookRendererGL* m_pNotebookRenderer;
    LightPrePassRendererGL* m_pLightPrePassRenderer;
    // named apart from Renderer::m_ambientOcclusion, the on/off setting
    core::SharedPtr<ScreenSpaceAmbientOcclusionGL> m_pAmbientOcclusion;
    core::SharedPtr<FogFilterGL> m_fogFilter;
    core::SharedPtr<TonemapperGL> m_tonemapper;
    core::SharedPtr<WaterRefractionGL> m_waterRefraction;
    core::SharedPtr<ShaderProgramGL> m_blitProgram;
    TextureGL* m_pRefractionBuffer;
};

} // namespace engine
