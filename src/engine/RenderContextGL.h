// OpenGL context helpers, reconstructed from RenderContextGL.cpp (0x08117ec0-0x0811a300)
// and the texture/shader helpers that live in LightPrePassRendererGL.cpp.
#pragma once
#include "core/Matrix.h"
#include "engine/Material.h"
#include <GL/glew.h>

namespace engine
{

class RenderableMeshGL;
class MeshEntity;

// Throws core::Exception describing glGetError() (0x08122350).
void checkGLErrors(const char* what);

class TextureGL
{
  public:
    TextureGL() : m_handle(0), m_target(GL_TEXTURE_2D), m_width(0), m_height(0), m_mipLevels(1) {}
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
    int m_mipLevels;
};

class Texture2DGL : public TextureGL
{
  public:
    // 0x08122a40: allocates level 0 of the given internal format.
    Texture2DGL(int width, int height, int mipLevels, GLint internalFormat, GLint minFilter,
                GLint magFilter, GLint wrap);
    // Wraps an already created texture object.
    Texture2DGL(GLuint handle, int width, int height, int mipLevels);
    ~Texture2DGL();
};

class TextureCubeGL : public TextureGL
{
  public:
    // 0x08122bd0
    TextureCubeGL(int size, int mipLevels, GLint internalFormat);
    ~TextureCubeGL();
};

// A linked GLSL program with the attribute and uniform tables of the engine bound.
class ShaderProgramGL
{
  public:
    enum Uniform
    {
        U_modelViewProj = 0,
        U_modelView,
        U_proj,
        U_texture,
        U_diffuseMap,
        U_normalMap,
        U_specularMap,
        U_glossiness,
        U_texOffset,
        U_skinningMatrices,
        U_localToWorld,
        U_fadeParms,
        U_textureAnimParms,
        U_gravity,
        U_airResistance,
        U_rotationSpeed,
        U_clampToGroundPlane,
        U_depthBias,
        U_colorTable,
        U_opacity,
        U_lightColor,
        NumUniforms = 21
    };
    ShaderProgramGL(GLuint vertexShader, GLuint fragmentShader);
    ShaderProgramGL(const char* vertexFile, const char* fragmentFile);
    ~ShaderProgramGL();
    GLuint getProgram() const
    {
        return m_program;
    }
    GLint getUniform(int i) const
    {
        return m_uniforms[i];
    }
    void validateProgram();

  private:
    void linkProgram(GLuint program);
    void bindAttributesAndUniforms();
    GLuint m_program;
    GLint m_uniforms[NumUniforms];
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
        Blend_Premultiplied = 4
    };
    static constexpr int StreamBufferSize = 0x100000; // bytes per streaming VBO
    static constexpr int NumQuads = 16384; // quads addressable by the shared index buffer
    enum Primitive
    {
        Prim_Points = 0,
        Prim_Lines = 1,
        Prim_Triangles = 2,
        Prim_Quads = 3
    };

    RenderContextGL();
    virtual ~RenderContextGL();
    virtual void setRenderTarget(FrameBuffer_t fb);
    virtual void resizeWindow(int width, int height);

    void contextCreated();
    void contextTerminating();
    void setRenderTarget(Texture2DGL* color0, Texture2DGL* color1, GLuint depthBuffer,
                         GLuint stencilBuffer);
    void setBlendMode(int mode);
    void setScissorTest(bool on);
    void drawArrays(int primitive, int first, int count);
    void drawRect();
    void drawMesh(RenderableMeshGL& mesh);
    void useProgram(ShaderProgramGL* program);
    ShaderProgramGL* getProgram() const
    {
        return m_pProgram;
    }
    void resetTextureUnits()
    {
        m_textureUnit = 0;
    }
    void setShaderParams(const Material& material);
    void setUniformTexture(const char* name, TextureGL* texture, int filter, int address);
    // Same through the uniform table of the current program.
    void setUniformTexture(int uniform, TextureGL* texture, int filter, int address);
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
    void setTextureFilterAndWrapMode(TextureGL* texture, int filter, int address);
    void setSkinningMatrices(const MeshEntity& entity);
    void* streamWrite(int bytes);
    void streamDrawPrimitive(int primitive, int numPrimitives);
    const unsigned short* getQuadIndices() const
    {
        return m_pQuadIndices;
    }
    float getMaxAnisotropy() const
    {
        return m_maxAnisotropy;
    }
    int getBlendMode() const
    {
        return m_blendMode;
    }

    static GLuint compileShader(const char* source, GLenum type, const char* const* defines,
                                const char* name);
#if GRIMROCK_GAME >= 2
    // grimrock2.exe 0x004e20e0: expands #include "file" (relative to the including file,
    // then to shaderDir) with #line directives; includeCount numbers the files.
    static void preprocessShader(const char* filename, const char* shaderDir, core::String& out,
                                 int& includeCount);
#endif
    static GLuint compileShaderFromFile(const char* filename, GLenum type,
                                        const char* const* defines);
    // D3D clip space (z in [0,1]) to GL clip space (z in [-1,1]).
    static core::Matrix4x4 sm_d3dToGLProj;

  protected:
    float m_maxAnisotropy;
    ShaderProgramGL* m_pProgram;
    int m_blendMode;
    char* m_pStreamBuffer;
    unsigned short* m_pQuadIndices;
    int m_textureUnit;
    GLuint m_frameBuffer;
    GLuint m_defaultFrameBuffer;
};

} // namespace engine
