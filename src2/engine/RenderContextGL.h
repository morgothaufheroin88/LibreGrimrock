// OpenGL 3.2 context helpers of Legend of Grimrock 2, reconstructed from grimrock2.exe
// RenderContextGL.cpp (0x004e0c80-0x004e2560) and the texture helpers of RendererGL.cpp
// (0x004c4140-0x004c4460). Unlike the first game the shaders are GLSL 1.50 with explicit
// attribute/fragment bindings, meshes live in vertex array objects and the immediate mode
// streams through a mapped buffer.
#pragma once
#include "core/Matrix.h"
#include "engine/Material.h"
#include <GL/glew.h>

namespace engine
{

class RenderableMeshGL;
class MeshEntity;
class Scene;
class Camera;

// Prints and throws core::Exception describing glGetError() (0x004c4140).
void checkGLErrors(const char* what);

class TextureGL
{
  public:
    TextureGL()
        : m_handle(0), m_target(GL_TEXTURE_2D), m_width(0), m_height(0), m_depth(0), m_mipLevels(1)
    {
    }
    // 0x004c41d0
    virtual ~TextureGL();
    GLuint getHandle() const
    {
        return m_handle;
    }
    GLenum getTarget() const
    {
        return m_target;
    }
    int getWidth() const
    {
        return m_width;
    }
    int getHeight() const
    {
        return m_height;
    }
    int getDepth() const
    {
        return m_depth;
    }
    int getMipLevels() const
    {
        return m_mipLevels;
    }

  protected:
    friend class RenderContextGL;
    friend class RenderableTextureGL;
    GLuint m_handle;
    GLenum m_target;
    int m_width;
    int m_height;
    int m_depth;
    int m_mipLevels;
};

class Texture2DGL : public TextureGL
{
  public:
    // 0x004c4220: allocates level 0 of the given internal format.
    Texture2DGL(int width, int height, int mipLevels, GLint internalFormat, GLint minFilter,
                GLint magFilter, GLint wrap, GLenum format);
    // Wraps an already created texture object.
    Texture2DGL(GLuint handle, int width, int height, int mipLevels);
    ~Texture2DGL();
};

class Texture3DGL : public TextureGL
{
  public:
    // 0x004e34a0
    Texture3DGL(int width, int height, int depth, int mipLevels, GLint internalFormat,
                GLint minFilter, GLint magFilter, GLint wrap, GLenum format);
    ~Texture3DGL();
};

class TextureCubeGL : public TextureGL
{
  public:
    TextureCubeGL(int size, int mipLevels, GLint internalFormat);
    ~TextureCubeGL();
};

// A linked GLSL program with the attribute, fragment output and uniform tables of the
// engine bound (0x84 bytes: program, 31 uniform locations, registry index).
class ShaderProgramGL
{
  public:
    enum Uniform
    {
        U_modelViewProj = 0,
        U_modelView,
        U_model,
        U_proj,
        U_texture,
        U_diffuseMap,
        U_normalMap,
        U_specularMap,
        U_emissiveMap,
        U_dissolveMap,
        U_dissolve,
        U_glossiness,
        U_texOffset,
        U_texcoordScaleOffset,
        U_skinningMatrices,
        U_localToWorld,
        U_fadeParms,
        U_textureAnimParms,
        U_gravity,
        U_airResistance,
        U_rotationParams,
        U_clampToGroundPlane,
        U_depthBias,
        U_colorTable,
        U_opacity,
        U_lightColor,
        U_invLightRange,
        U_emissiveColor,
        U_invScreenSize,
        U_lightBuffer,
        U_geometryBuffer,
        NumUniforms = 31
    };
    static constexpr int NumAttributes = 10;

    // 0x004e2000
    ShaderProgramGL(GLuint vertexShader, GLuint fragmentShader);
    // 0x004e2560
    ShaderProgramGL(const char* vertexFile, const char* fragmentFile);
    // 0x004e1e90
    ~ShaderProgramGL();
    GLuint getProgram() const
    {
        return m_program;
    }
    GLint getUniform(int i) const
    {
        return m_uniforms[i];
    }
    // position in sm_programs, the sort key of the renderers (+0x80)
    int getRegistryIndex() const
    {
        return m_registryIndex;
    }
    // All programs ever linked, for the shader statistics (0x006201b4/0x006201b8).
    static core::Array<ShaderProgramGL*> sm_programs;

  private:
    // 0x004e1ee0
    static void linkProgram(GLuint program);
    void bindAttributesAndUniforms();
    GLuint m_program;
    GLint m_uniforms[NumUniforms];
    unsigned short m_registryIndex;
};

class RenderContextGL
{
  public:
    enum FrameBuffer_t
    {
        DefaultFrameBuffer = 0
    };
    enum BlendMode
    {
        Blend_Opaque = 0,
        Blend_Additive = 1,
        Blend_Modulative = 2,
        Blend_Translucent = 3,
        Blend_Screen = 4,
        Blend_AdditiveSrcAlpha = 5,
        Blend_PremultipliedAlpha = 6
    };
    static constexpr int StreamBufferSize = 0x100000; // bytes of the streaming VBO
    static constexpr int NumQuads = 16384;            // quads addressable by the index buffer
    static constexpr float MaxAnisotropy = 8.0f;
    static constexpr float RequiredGLVersion = 3.2f;
    enum Primitive
    {
        Prim_Points = 0,
        Prim_Lines = 1,
        Prim_Triangles = 2,
        Prim_Quads = 3
    };

    // 0x004e1350
    RenderContextGL();
    // 0x004e1690
    virtual ~RenderContextGL();
    // 0x004e0c80
    virtual void setRenderTarget(FrameBuffer_t fb);
    virtual void resizeWindow(int width, int height);
    virtual void swapBuffers() = 0;

    // 0x004e16c0: requires GL 3.2 and two draw buffers; creates the streaming buffers.
    void contextCreated();
    // 0x004e13c0
    void contextTerminating();
    // 0x004c42e0
    void setRenderTarget(Texture2DGL* color0, Texture2DGL* color1, GLuint depthBuffer,
                         GLuint stencilBuffer);
    // 0x004e5ba0: one face of a cube map as the colour target.
    void setRenderTarget(TextureCubeGL* cube, int face, GLuint depthBuffer, GLuint stencilBuffer);
    // 0x004c43c0
    void setBlendMode(int mode);
    // 0x004e1410: full screen quad through attribute 0.
    void drawRect();
    // 0x004e1c10
    void drawMesh(RenderableMeshGL& mesh);
    void useProgram(ShaderProgramGL* program);
    ShaderProgramGL* getProgram() const
    {
        return m_pProgram;
    }
    // 0x004e1a80: material parameters as uniforms, textures from the given unit on.
    void setShaderParams(const Material& material, int firstTextureUnit);
    // 0x004e1630
    void setUniformTexture(const char* name, TextureGL* texture, int filter, int address, int unit);
    void setUniformTexture(int uniform, TextureGL* texture, int filter, int address, int unit);
    GLuint getFrameBuffer() const
    {
        return m_frameBuffer;
    }
    // Framebuffer that stands in for the window (0 unless the frame is scaled to the
    // window, see RenderContextSDL).
    GLuint getDefaultFrameBuffer() const
    {
        return m_defaultFrameBuffer;
    }
    // 0x004e1550
    static void setTextureFilterAndWrapMode(TextureGL* texture, int filter, int address);
    // 0x004e1a10
    void setSkinningMatrices(const MeshEntity& entity);
    // 0x004e1430: maps bytes of the streaming buffer, orphaning it when full.
    void* streamWrite(int bytes);
    // 0x004e14b0: unmaps and draws what streamWrite returned.
    void streamDrawPrimitive(int primitive, int count);
    GLuint getStreamBuffer() const
    {
        return m_streamBuffer;
    }
    int getStreamOffset() const
    {
        return m_streamOffset;
    }
    int getBlendMode() const
    {
        return m_blendMode;
    }
    int getFrameCounter() const
    {
        return m_frameCounter;
    }
    void nextFrame()
    {
        ++m_frameCounter;
    }
    void setScene(Scene* scene, Camera* camera)
    {
        m_pScene = scene;
        m_pCamera = camera;
    }
    Scene* getScene() const
    {
        return m_pScene;
    }
    Camera* getCamera() const
    {
        return m_pCamera;
    }
    static float getMaxAnisotropy()
    {
        return sm_maxAnisotropy;
    }

    // 0x004e1c70: strips CRs and inserts "#define X 1" lines after the #version line;
    // returns 0 and the log on failure.
    static GLuint compileShader(const char* source, GLenum type, const char* const* defines,
                                core::String& log);
    // 0x004e20e0: expands #include "file" (relative to the including file, then to
    // shaderDir) with #line directives; includeCount numbers the files.
    static void preprocessShader(const char* filename, const char* shaderDir, core::String& out,
                                 int& includeCount);
    // 0x004e2470
    static GLuint compileShaderFromFile(const char* filename, GLenum type,
                                        const char* const* defines);
    // D3D clip space (z in [0,1]) to GL clip space (z in [-1,1]) (0x006173b0).
    static core::Matrix4x4 sm_d3dToGLProj;
    // GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT capped at MaxAnisotropy, 0 without the extension
    // (0x006201b0).
    static float sm_maxAnisotropy;

  protected:
    // 0x004e12e0
    static bool hasExtension(const char* name);
    int m_frameCounter;
    ShaderProgramGL* m_pProgram;
    int m_blendMode;
    GLuint m_streamBuffer;
    int m_streamOffset;
    GLuint m_quadIndexBuffer;
    GLuint m_frameBuffer;
    GLuint m_rectBuffer;
    GLuint m_rectVertexArray;
    Scene* m_pScene;
    Camera* m_pCamera;
    GLuint m_defaultFrameBuffer;
};

} // namespace engine
