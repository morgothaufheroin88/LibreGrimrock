// Reconstructed from Grimrock.bin.x86 RenderContextGL.cpp (plus the GL helpers found in
// LightPrePassRendererGL.cpp).
#include "engine/RenderContextGL.h"
#include "core/Exception.h"
#include "core/FileSystem.h"
#include "core/Sys.h"
#include "engine/RenderEntity.h"
#include "engine/RendererGL.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace engine
{

using namespace core;

// 0x08122350
void checkGLErrors(const char* what)
{
    GLenum err = glGetError();
    if (err == GL_NO_ERROR)
        return;
    const char* name;
    switch (err)
    {
    case GL_INVALID_ENUM:
        name = "GL_INVALID_ENUM";
        break;
    case GL_INVALID_VALUE:
        name = "GL_INVALID_VALUE";
        break;
    case GL_INVALID_OPERATION:
        name = "GL_INVALID_OPERATION";
        break;
    case GL_STACK_OVERFLOW:
        name = "GL_STACK_OVERFLOW";
        break;
    case GL_STACK_UNDERFLOW:
        name = "GL_STACK_UNDERFLOW";
        break;
    case GL_OUT_OF_MEMORY:
        name = "GL_OUT_OF_MEMORY";
        break;
    default:
        name = "unknown error";
        break;
    }
    throw Exception("GL error: %s failed (%s 0x%04X)", what, name, err);
}

// ---- textures --------------------------------------------------------------------

// 0x08114ac0
TextureGL::~TextureGL()
{
    if (m_handle)
        glDeleteTextures(1, &m_handle);
}
// 0x08122a40
Texture2DGL::Texture2DGL(int width, int height, int mipLevels, GLint internalFormat,
                         GLint minFilter, GLint magFilter, GLint wrap)
{
    m_target = GL_TEXTURE_2D;
    m_width = width;
    m_height = height;
    m_mipLevels = mipLevels;
    glGenTextures(1, &m_handle);
    glBindTexture(GL_TEXTURE_2D, m_handle);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
}
Texture2DGL::Texture2DGL(GLuint handle, int width, int height, int mipLevels)
{
    m_target = GL_TEXTURE_2D;
    m_handle = handle;
    m_width = width;
    m_height = height;
    m_mipLevels = mipLevels;
}
Texture2DGL::~Texture2DGL() {}
// 0x08122bd0
TextureCubeGL::TextureCubeGL(int size, int mipLevels, GLint internalFormat)
{
    m_target = GL_TEXTURE_CUBE_MAP;
    m_width = size;
    m_height = size;
    m_mipLevels = mipLevels;
    glGenTextures(1, &m_handle);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_handle);
    for (int face = 0; face < 6; ++face)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, internalFormat, size, size, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    checkGLErrors("create cube map");
}
TextureCubeGL::~TextureCubeGL() {}

// ---- shaders ---------------------------------------------------------------------

static constexpr const char* g_attribNames[ShaderProgramGL::NumAttributes] = {
    "position", "normal",      "tangent",     "bitangent", "texcoord",
    "color",    "boneIndices", "boneWeights", "velocity",  "particleParms"};
static constexpr const char* g_uniformNames[ShaderProgramGL::NumUniforms] = {
    "modelViewProj",      "modelView",        "proj",          "texture",
    "diffuseMap",         "normalMap",        "specularMap",   "glossiness",
    "texOffset",          "skinningMatrices", "localToWorld",  "fadeParms",
    "textureAnimParms",   "gravity",          "airResistance", "rotationSpeed",
    "clampToGroundPlane", "depthBias",        "colorTable",    "opacity",
    "lightColor"};

void ShaderProgramGL::bindAttributesAndUniforms()
{
    for (int i = 0; i < NumAttributes; ++i)
        glBindAttribLocation(m_program, i, g_attribNames[i]);
    linkProgram(m_program);
    for (int i = 0; i < NumUniforms; ++i)
        m_uniforms[i] = glGetUniformLocation(m_program, g_uniformNames[i]);
}
// 0x08119f50
ShaderProgramGL::ShaderProgramGL(GLuint vertexShader, GLuint fragmentShader)
{
    m_program = glCreateProgram();
    glAttachShader(m_program, vertexShader);
    glAttachShader(m_program, fragmentShader);
    bindAttributesAndUniforms();
}
// 0x0811a0e0
ShaderProgramGL::ShaderProgramGL(const char* vertexFile, const char* fragmentFile)
{
    m_program = glCreateProgram();
    GLuint vs = RenderContextGL::compileShaderFromFile(vertexFile, GL_VERTEX_SHADER, 0);
    GLuint fs = RenderContextGL::compileShaderFromFile(fragmentFile, GL_FRAGMENT_SHADER, 0);
    glAttachShader(m_program, vs);
    glAttachShader(m_program, fs);
    bindAttributesAndUniforms();
    if (vs)
        glDeleteShader(vs);
    if (fs)
        glDeleteShader(fs);
}
ShaderProgramGL::~ShaderProgramGL()
{
    if (m_program)
        glDeleteProgram(m_program);
}
// 0x08119de0
void ShaderProgramGL::linkProgram(GLuint program)
{
    glLinkProgram(program);
    GLint logLength = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
    String log;
    if (logLength > 0)
    {
        char* buffer = (char*)malloc(logLength);
        glGetProgramInfoLog(program, logLength, &logLength, buffer);
        if (logLength > 0)
            log = buffer;
        free(buffer);
    }
    GLint status = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (!status)
        throw Exception("Failed to link shader program\n%s", log.c_str());
}
// 0x081197f0
void ShaderProgramGL::validateProgram()
{
    glValidateProgram(m_program);
    GLint logLength = 0;
    glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &logLength);
    String log;
    if (logLength > 0)
    {
        char* buffer = (char*)malloc(logLength);
        glGetProgramInfoLog(m_program, logLength, &logLength, buffer);
        debugPrint("Program validate log:\n%s", buffer);
        log = buffer;
        free(buffer);
    }
    GLint status = 0;
    glGetProgramiv(m_program, GL_VALIDATE_STATUS, &status);
    if (!status)
        throw Exception("Failed to validate shader program\n%s", log.c_str());
}

// ---- RenderContextGL -------------------------------------------------------------

Matrix4x4 RenderContextGL::sm_d3dToGLProj;

// 0x08118ad0
RenderContextGL::RenderContextGL()
    : m_maxAnisotropy(0.0f), m_pProgram(0), m_blendMode(0), m_textureUnit(0), m_frameBuffer(0),
      m_defaultFrameBuffer(0)
{
    // z' = 2z - 1 (row 2 = (0, 0, 2, -1))
    sm_d3dToGLProj.makeIdentity();
    sm_d3dToGLProj.m[10] = 2.0f;
    sm_d3dToGLProj.m[14] = -1.0f;
    m_pStreamBuffer = new char[StreamBufferSize];
    m_pQuadIndices = new unsigned short[NumQuads * 6];
    unsigned short v = 0;
    for (int i = 0; i < NumQuads; ++i, v += 4)
    {
        m_pQuadIndices[i * 6 + 0] = v;
        m_pQuadIndices[i * 6 + 1] = v + 1;
        m_pQuadIndices[i * 6 + 2] = v + 2;
        m_pQuadIndices[i * 6 + 3] = v;
        m_pQuadIndices[i * 6 + 4] = v + 2;
        m_pQuadIndices[i * 6 + 5] = v + 3;
    }
}
// 0x08118200
RenderContextGL::~RenderContextGL()
{
    delete[] m_pStreamBuffer;
    delete[] m_pQuadIndices;
}
// 0x081100a0
void RenderContextGL::setRenderTarget(FrameBuffer_t fb)
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_defaultFrameBuffer);
}
// 0x08110090
void RenderContextGL::resizeWindow(int width, int height) {}

// 0x08118850
void RenderContextGL::contextCreated()
{
    glGenFramebuffers(1, &m_frameBuffer);
    const char* version = (const char*)glGetString(GL_VERSION);
    debugPrint("GL_VERSION: %s\n", version ? version : "");
    float versionNumber = 0.0f;
    if (version)
        sscanf(version, "%f", &versionNumber);
    const char* vendor = (const char*)glGetString(GL_VENDOR);
    debugPrint("GL_VENDOR: %s\n", vendor ? vendor : "");
    const char* renderer = (const char*)glGetString(GL_RENDERER);
    debugPrint("GL_RENDERER: %s\n", renderer ? renderer : "");
    const char* glsl = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
    debugPrint("GL_SHADING_LANGUAGE_VERSION: %s\n", glsl ? glsl : "");
    const char* extensions = (const char*)glGetString(GL_EXTENSIONS);
    debugPrint("GL_EXTENSIONS: %s\n", extensions ? extensions : "");
    GLint stencilBits = 0;
    glGetIntegerv(GL_STENCIL_BITS, &stencilBits);
    debugPrint("GL_STENCIL_BITS: %d\n", stencilBits);
    if (stencilBits < 1)
        throw Exception("Stencil buffer not supported");
    GLint maxUniforms = 0;
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &maxUniforms);
    debugPrint("GL_MAX_VERTEX_UNIFORM_COMPONENTS: %d\n", maxUniforms);
    GLint maxDrawBuffers = 0;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
    if (maxDrawBuffers < 2)
        throw Exception("Multiple render target rendering not supported");
    if (!GLEW_EXT_texture_compression_s3tc)
        throw Exception("S3TC is not supported");
    m_maxAnisotropy = 0.0f;
    if (extensions && strstr(extensions, "GL_EXT_texture_filter_anisotropic"))
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &m_maxAnisotropy);
    debugPrint("GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT: %f\n", (double)m_maxAnisotropy);
}
// 0x08117ec0
void RenderContextGL::contextTerminating()
{
    glDeleteFramebuffers(1, &m_frameBuffer);
}
// 0x08122430
void RenderContextGL::setRenderTarget(Texture2DGL* color0, Texture2DGL* color1, GLuint depthBuffer,
                                      GLuint stencilBuffer)
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_frameBuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              stencilBuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           color0 ? color0->getHandle() : 0, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D,
                           color1 ? color1->getHandle() : 0, 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        throw Exception("Could not set render targets (glCheckFramebufferStatus returned %d)",
                        status);
}
// 0x08122580
void RenderContextGL::setBlendMode(int mode)
{
    if (m_blendMode == mode)
        return;
    if (mode == Blend_Opaque)
    {
        glDisable(GL_BLEND);
    }
    else
    {
        static constexpr GLenum table[5][2] = {{GL_ZERO, GL_ZERO},
                                               {GL_ONE, GL_ONE},
                                               {GL_DST_COLOR, GL_ZERO},
                                               {GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA},
                                               {GL_SRC_ALPHA, GL_ONE}};
        glEnable(GL_BLEND);
        glBlendFunc(table[mode][0], table[mode][1]);
    }
    m_blendMode = mode;
}
// 0x08122320
void RenderContextGL::setScissorTest(bool on)
{
    if (on)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);
}
// 0x081183d0
// primitive: 0 points, 1 lines, 2 triangles, 3 quads.
static constexpr GLenum g_primitiveModes[4] = {GL_POINTS, GL_LINES, GL_TRIANGLES, GL_QUADS};
void RenderContextGL::drawArrays(int primitive, int first, int count)
{
    glDrawArrays(g_primitiveModes[primitive], first, count);
}
// 0x08118bf0: full screen quad through attribute 0.
void RenderContextGL::drawRect()
{
    static constexpr float verts[8] = {-1, 1, 1, 1, 1, -1, -1, -1};
    glEnableVertexAttribArray(ShaderProgramGL::A_position);
    glVertexAttribPointer(ShaderProgramGL::A_position, 2, GL_FLOAT, GL_FALSE, 8, verts);
    glDrawArrays(GL_QUADS, 0, 4);
    glDisableVertexAttribArray(ShaderProgramGL::A_position);
}
// 0x08118410
void RenderContextGL::drawMesh(RenderableMeshGL& mesh)
{
    mesh.activate();
    for (int i = 0; i < mesh.getNumSegments(); ++i)
        mesh.renderSegment(i);
    for (int i = 0; i < ShaderProgramGL::NumAttributes; ++i)
        glDisableVertexAttribArray(i);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}
// Inlined everywhere: switch program and restart texture unit allocation.
void RenderContextGL::useProgram(ShaderProgramGL* program)
{
    if (program != m_pProgram)
    {
        glUseProgram(program ? program->getProgram() : 0);
        m_pProgram = program;
    }
    m_textureUnit = 0;
}
// 0x08118240: filter table indexed by Material::TextureFilter; textures without mip
// levels fall back to plain nearest/linear.
void RenderContextGL::setTextureFilterAndWrapMode(TextureGL* texture, int filter, int address)
{
    if (filter >= 0)
    {
        static constexpr GLint filters[6][2] = {{GL_NEAREST, GL_NEAREST},
                                                {GL_LINEAR, GL_LINEAR},
                                                {GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
                                                {GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
                                                {GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR},
                                                {GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}};
        if (texture->getMipLevels() == 1)
            filter =
                (filter != Material::Nearest_MipNearest && filter != Material::Nearest) ? 1 : 0;
        glTexParameteri(texture->getTarget(), GL_TEXTURE_MIN_FILTER, filters[filter][0]);
        glTexParameteri(texture->getTarget(), GL_TEXTURE_MAG_FILTER, filters[filter][1]);
        if (m_maxAnisotropy > 0.0f)
            glTexParameterf(texture->getTarget(), GL_TEXTURE_MAX_ANISOTROPY_EXT,
                            filter == Material::Anisotropic ? m_maxAnisotropy : 1.0f);
    }
    if (address >= 0)
    {
        GLint wrap = address == Material::Wrap ? GL_REPEAT : GL_CLAMP_TO_EDGE;
        glTexParameteri(texture->getTarget(), GL_TEXTURE_WRAP_S, wrap);
        glTexParameteri(texture->getTarget(), GL_TEXTURE_WRAP_T, wrap);
        glTexParameteri(texture->getTarget(), GL_TEXTURE_WRAP_R, wrap);
    }
}
// 0x08122910
void RenderContextGL::setUniformTexture(const char* name, TextureGL* texture, int filter,
                                        int address)
{
    int unit = m_textureUnit++;
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(texture->getTarget(), texture->getHandle());
    GLint location = glGetUniformLocation(m_pProgram->getProgram(), name);
    glUniform1i(location, unit);
    setTextureFilterAndWrapMode(texture, filter, address);
}
void RenderContextGL::setUniformTexture(int uniform, TextureGL* texture, int filter, int address)
{
    int unit = m_textureUnit++;
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(texture->getTarget(), texture->getHandle());
    glUniform1i(m_pProgram->getUniform(uniform), unit);
    setTextureFilterAndWrapMode(texture, filter, address);
}
// 0x08118500
void RenderContextGL::setShaderParams(const Material& material)
{
    for (int i = 0; i < material.getParamCount(); ++i)
    {
        const Material::ShaderParam& p = material.getParam(i);
        GLint location;
        switch (p.type)
        {
        case Material::ParamTexture:
            if (p.texture)
            {
                TextureGL* textureGL = ((RenderableTextureGL*)p.texture.get())->getTexture();
                int unit = m_textureUnit++;
                glActiveTexture(GL_TEXTURE0 + unit);
                glBindTexture(textureGL->getTarget(), textureGL->getHandle());
                location = glGetUniformLocation(m_pProgram->getProgram(), p.name.c_str());
                glUniform1i(location, unit);
                setTextureFilterAndWrapMode(textureGL, p.filter, p.address);
            }
            break;
        case Material::ParamFloat:
            location = glGetUniformLocation(m_pProgram->getProgram(), p.name.c_str());
            glUniform1f(location, p.value.x);
            break;
        case Material::ParamVec2:
            location = glGetUniformLocation(m_pProgram->getProgram(), p.name.c_str());
            glUniform2f(location, p.value.x, p.value.y);
            break;
        case Material::ParamVec3:
            location = glGetUniformLocation(m_pProgram->getProgram(), p.name.c_str());
            glUniform3f(location, p.value.x, p.value.y, p.value.z);
            break;
        case Material::ParamVec4:
            location = glGetUniformLocation(m_pProgram->getProgram(), p.name.c_str());
            glUniform4f(location, p.value.x, p.value.y, p.value.z, p.value.w);
            break;
        }
    }
}
// 0x08119000: skinning matrices (model space) uploaded as 3 vec4 rows per bone.
void RenderContextGL::setSkinningMatrices(const MeshEntity& entity)
{
    static float skinningMatrices[64 * 12];
    const Skeleton* skeleton = entity.getSkeleton();
    const Matrix4x3& worldToModel = entity.getNode()->getWorldToLocalMatrix();
    int numBones = skeleton->getBoneCount();
    if (numBones > 64)
        numBones = 64;
    for (int i = 0; i < numBones; ++i)
    {
        Matrix4x3 m = worldToModel * (skeleton->getBone(i)->getLocalToWorldMatrix() *
                                      skeleton->getInvBindMatrix(i));
        float* out = skinningMatrices + i * 12;
        out[0] = m.x.x;
        out[1] = m.y.x;
        out[2] = m.z.x;
        out[3] = m.pos.x;
        out[4] = m.x.y;
        out[5] = m.y.y;
        out[6] = m.z.y;
        out[7] = m.pos.y;
        out[8] = m.x.z;
        out[9] = m.y.z;
        out[10] = m.z.z;
        out[11] = m.pos.z;
    }
    m_pProgram->setUniform4v(ShaderProgramGL::U_skinningMatrices, skinningMatrices, numBones * 3);
}
// 0x08117ee0
void* RenderContextGL::streamWrite(int bytes)
{
    if (bytes > StreamBufferSize)
        return 0;
    return m_pStreamBuffer;
}
// 0x08118c90
void RenderContextGL::streamDrawPrimitive(int primitive, int numPrimitives)
{
    static constexpr int verticesPerPrimitive[4] = {1, 2, 3, 4};
    glDrawArrays(g_primitiveModes[primitive], 0, numPrimitives * verticesPerPrimitive[primitive]);
}
// 0x08119a10: strips CRs and inserts "#define X 1" lines after the #version line.
GLuint RenderContextGL::compileShader(const char* source, GLenum type, const char* const* defines,
                                      const char* name)
{
    String text(source);
    text.replace("\r", "");
    GLuint shader = glCreateShader(type);
    if (defines)
    {
        int pos = text.find("#version", 0);
        if (pos < 0)
        {
            pos = 0;
        }
        else
        {
            while (text[pos] != '\n')
            {
                if (text[pos] == 0)
                    break;
                ++pos;
            }
            if (text[pos] == '\n')
                ++pos;
        }
        for (const char* const* define = defines; *define; ++define)
        {
            String line = formatString("#define %s 1\n", *define);
            text.insert(pos, line);
            pos += line.size();
        }
    }
    const char* src = text.c_str();
    glShaderSource(shader, 1, &src, 0);
    glCompileShader(shader);
    GLint logLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    String log;
    if (logLength > 0)
    {
        char* buffer = (char*)malloc(logLength);
        glGetShaderInfoLog(shader, logLength, &logLength, buffer);
        debugPrint("%s", buffer);
        log = buffer;
        free(buffer);
    }
    GLint status = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status)
    {
        glDeleteShader(shader);
        throw Exception("Failed to compile shader %s\n%s", name ? name : "<string>", log.c_str());
    }
    return shader;
}
// 0x08119d30
GLuint RenderContextGL::compileShaderFromFile(const char* filename, GLenum type,
                                              const char* const* defines)
{
    debugPrint("Compiling shader %s\n", filename);
    int length = 0;
    char* source = readFile(filename, length);
    if (!source)
        throw Exception("Failed to load shader file %s", filename);
    GLuint shader = compileShader(source, type, defines, filename);
    delete[] source;
    return shader;
}

} // namespace engine
