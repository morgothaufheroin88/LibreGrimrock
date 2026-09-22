// Reconstructed from grimrock2.exe RenderContextGL.cpp (0x004e0c80-0x004e2560) and the GL
// helpers of RendererGL.cpp (0x004c4140-0x004c4460).
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

// 0x004c4140
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
    case GL_OUT_OF_MEMORY:
        name = "GL_OUT_OF_MEMORY";
        break;
    default:
        name = "unknown error";
        break;
    }
    printf("GL error: %s failed (%s 0x%04X)\n", what, name, err);
    throw Exception("GL error: %s failed (%s 0x%04X)", what, name, err);
}

// ---- textures --------------------------------------------------------------------

// 0x004c41d0
TextureGL::~TextureGL()
{
    glDeleteTextures(1, &m_handle);
}
// 0x004c4220
Texture2DGL::Texture2DGL(int width, int height, int mipLevels, GLint internalFormat,
                         GLint minFilter, GLint magFilter, GLint wrap, GLenum format)
{
    m_target = GL_TEXTURE_2D;
    m_width = width;
    m_height = height;
    m_depth = 0;
    m_mipLevels = mipLevels;
    glGenTextures(1, &m_handle);
    glBindTexture(GL_TEXTURE_2D, m_handle);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, 0);
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
    m_depth = 0;
    m_mipLevels = mipLevels;
}
Texture2DGL::~Texture2DGL() {}
// 0x004e34a0
Texture3DGL::Texture3DGL(int width, int height, int depth, int mipLevels, GLint internalFormat,
                         GLint minFilter, GLint magFilter, GLint wrap, GLenum format)
{
    m_target = GL_TEXTURE_3D;
    m_width = width;
    m_height = height;
    m_depth = depth;
    m_mipLevels = mipLevels;
    glGenTextures(1, &m_handle);
    glBindTexture(GL_TEXTURE_3D, m_handle);
    glTexImage3D(GL_TEXTURE_3D, 0, internalFormat, width, height, depth, 0, format,
                 GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, magFilter);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, wrap);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, wrap);
}
Texture3DGL::~Texture3DGL() {}
// 0x004e5a80
TextureCubeGL::TextureCubeGL(int size, int mipLevels, GLint internalFormat)
{
    m_target = GL_TEXTURE_CUBE_MAP;
    m_width = size;
    m_height = size;
    m_depth = 0;
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

// 0x00617290
static constexpr const char* g_attribNames[ShaderProgramGL::NumAttributes] = {
    "position", "normal",      "tangent",     "bitangent", "texcoord",
    "color",    "boneIndices", "boneWeights", "velocity",  "particleParms"};
// 0x006172b8
static constexpr const char* g_uniformNames[ShaderProgramGL::NumUniforms] = {
    "g_modelViewProj",
    "g_modelView",
    "g_model",
    "g_proj",
    "g_texture",
    "g_diffuseMap",
    "g_normalMap",
    "g_specularMap",
    "g_emissiveMap",
    "g_dissolveMap",
    "g_dissolve",
    "g_glossiness",
    "g_texOffset",
    "g_texcoordScaleOffset",
    "g_skinningMatrices",
    "g_localToWorld",
    "g_fadeParms",
    "g_textureAnimParms",
    "g_gravity",
    "g_airResistance",
    "g_rotationParams",
    "g_clampToGroundPlane",
    "g_depthBias",
    "g_colorTable",
    "g_opacity",
    "g_lightColor",
    "g_invLightRange",
    "g_emissiveColor",
    "g_invScreenSize",
    "g_lightBuffer",
    "g_geometryBuffer"};

Array<ShaderProgramGL*> ShaderProgramGL::sm_programs;

void ShaderProgramGL::bindAttributesAndUniforms()
{
    m_registryIndex = (unsigned short)sm_programs.size();
    sm_programs.push_back(this);
    for (int i = 0; i < NumAttributes; ++i)
        glBindAttribLocation(m_program, i, g_attribNames[i]);
    glBindFragDataLocation(m_program, 0, "fragColor");
    glBindFragDataLocation(m_program, 1, "fragColor2");
    linkProgram(m_program);
    for (int i = 0; i < NumUniforms; ++i)
        m_uniforms[i] = glGetUniformLocation(m_program, g_uniformNames[i]);
}
// 0x004e2000
ShaderProgramGL::ShaderProgramGL(GLuint vertexShader, GLuint fragmentShader)
{
    m_program = glCreateProgram();
    glAttachShader(m_program, vertexShader);
    glAttachShader(m_program, fragmentShader);
    bindAttributesAndUniforms();
}
// 0x004e2560
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
// 0x004e1e90: the last program takes the slot of the removed one
ShaderProgramGL::~ShaderProgramGL()
{
    ShaderProgramGL* last = sm_programs[sm_programs.size() - 1];
    sm_programs[m_registryIndex] = last;
    last->m_registryIndex = m_registryIndex;
    if (sm_programs.size() > 0)
        sm_programs.resize(sm_programs.size() - 1);
    if (m_program)
        glDeleteProgram(m_program);
}
// 0x004e1ee0
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

// ---- RenderContextGL -------------------------------------------------------------

Matrix4x4 RenderContextGL::sm_d3dToGLProj;
float RenderContextGL::sm_maxAnisotropy = 0.0f;

// 0x004e1350
RenderContextGL::RenderContextGL()
    : m_frameCounter(0), m_pProgram(0), m_blendMode(0), m_streamBuffer(0), m_streamOffset(0),
      m_quadIndexBuffer(0), m_frameBuffer(0), m_rectBuffer(0), m_rectVertexArray(0), m_pScene(0),
      m_pCamera(0), m_defaultFrameBuffer(0)
{
    // z' = 2z - 1 (row 2 = (0, 0, 2, -1))
    sm_d3dToGLProj.makeIdentity();
    sm_d3dToGLProj.m[10] = 2.0f;
    sm_d3dToGLProj.m[14] = -1.0f;
}
// 0x004e1690
RenderContextGL::~RenderContextGL() {}
// 0x004e0c80
void RenderContextGL::setRenderTarget(FrameBuffer_t fb)
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_defaultFrameBuffer);
    checkGLErrors("setRenderTarget");
}
void RenderContextGL::resizeWindow(int width, int height) {}

// 0x004e12e0
bool RenderContextGL::hasExtension(const char* name)
{
    GLint count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &count);
    for (int i = 0; i < count; ++i)
    {
        const char* extension = (const char*)glGetStringi(GL_EXTENSIONS, i);
        if (extension && strcmp(extension, name) == 0)
            return true;
    }
    return false;
}
// 0x004e16c0
void RenderContextGL::contextCreated()
{
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
    debugPrint("GL_EXTENSIONS: ");
    GLint numExtensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
    for (int i = 0; i < numExtensions; ++i)
        debugPrint("%s ", (const char*)glGetStringi(GL_EXTENSIONS, i));
    debugPrint("\n");
    if (versionNumber < RequiredGLVersion)
        throw Exception("OpenGL 3.2 or higher required");
    GLint maxUniforms = 0;
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &maxUniforms);
    debugPrint("GL_MAX_VERTEX_UNIFORM_COMPONENTS: %d\n", maxUniforms);
    GLint maxDrawBuffers = 0;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
    if (maxDrawBuffers < 2)
        throw Exception("OpenGL 3.2 or higher required");
    sm_maxAnisotropy = 0.0f;
    if (hasExtension("GL_EXT_texture_filter_anisotropic"))
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &sm_maxAnisotropy);
    debugPrint("GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT: %f\n", (double)sm_maxAnisotropy);
    if (sm_maxAnisotropy >= MaxAnisotropy)
        sm_maxAnisotropy = MaxAnisotropy;

    glGenFramebuffers(1, &m_frameBuffer);
    // streaming vertex buffer of the immediate mode and particles
    glGenBuffers(1, &m_streamBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_streamBuffer);
    glBufferData(GL_ARRAY_BUFFER, StreamBufferSize, 0, GL_STREAM_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    // two triangles per quad
    glGenBuffers(1, &m_quadIndexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_quadIndexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, NumQuads * 6 * sizeof(unsigned short), 0, GL_STATIC_DRAW);
    unsigned short* indices = (unsigned short*)glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
    unsigned short v = 0;
    for (int i = 0; i < NumQuads; ++i, v += 4)
    {
        indices[i * 6 + 0] = v;
        indices[i * 6 + 1] = v + 1;
        indices[i * 6 + 2] = v + 2;
        indices[i * 6 + 3] = v;
        indices[i * 6 + 4] = v + 2;
        indices[i * 6 + 5] = v + 3;
    }
    glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    // full screen rectangle
    static constexpr float rect[8] = {-1, 1, 1, 1, 1, -1, -1, -1};
    glGenBuffers(1, &m_rectBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_rectBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(rect), rect, GL_STATIC_DRAW);
    glGenVertexArrays(1, &m_rectVertexArray);
    glBindVertexArray(m_rectVertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, m_rectBuffer);
    glEnableVertexAttribArray(ShaderProgramGL::A_position);
    glVertexAttribPointer(ShaderProgramGL::A_position, 2, GL_FLOAT, GL_FALSE, 8, 0);
    glBindVertexArray(0);
}
// 0x004e13c0
void RenderContextGL::contextTerminating()
{
    glDeleteFramebuffers(1, &m_frameBuffer);
    glDeleteBuffers(1, &m_streamBuffer);
    glDeleteBuffers(1, &m_quadIndexBuffer);
    glDeleteBuffers(1, &m_rectBuffer);
    glDeleteVertexArrays(1, &m_rectVertexArray);
}
// 0x004c42e0
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
    checkGLErrors("setRenderTarget");
}
// 0x004e5ba0
void RenderContextGL::setRenderTarget(TextureCubeGL* cube, int face, GLuint depthBuffer,
                                      GLuint stencilBuffer)
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_frameBuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              stencilBuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, cube->getHandle(), 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, 0, 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        throw Exception("Could not set render targets (glCheckFramebufferStatus returned %d)",
                        status);
    checkGLErrors("setRenderTarget");
}
// 0x004c43c0
void RenderContextGL::setBlendMode(int mode)
{
    static constexpr GLenum table[7][2] = {{GL_ZERO, GL_ZERO},
                                           {GL_ONE, GL_ONE},
                                           {GL_DST_COLOR, GL_ZERO},
                                           {GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA},
                                           {GL_ONE_MINUS_DST_COLOR, GL_ONE},
                                           {GL_SRC_ALPHA, GL_ONE},
                                           {GL_ONE, GL_ONE_MINUS_SRC_ALPHA}};
    if (m_blendMode == mode)
        return;
    if (mode == Blend_Opaque)
    {
        glDisable(GL_BLEND);
    }
    else
    {
        glEnable(GL_BLEND);
        glBlendFunc(table[mode][0], table[mode][1]);
    }
    m_blendMode = mode;
}
// 0x004e1410
void RenderContextGL::drawRect()
{
    glBindVertexArray(m_rectVertexArray);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}
// 0x004e1c10
void RenderContextGL::drawMesh(RenderableMeshGL& mesh)
{
    glBindVertexArray(mesh.getVertexArray());
    int indexSize = mesh.getIndexSize();
    for (int i = 0; i < mesh.getNumSegments(); ++i)
    {
        const RenderableMeshGL::Segment& segment = mesh.getSegment(i);
        glDrawElements(GL_TRIANGLES, segment.primitiveCount * 3,
                       indexSize == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
                       (const void*)(intptr_t)(segment.firstIndex * indexSize));
    }
}
// Inlined everywhere: switch program.
void RenderContextGL::useProgram(ShaderProgramGL* program)
{
    if (program != m_pProgram)
    {
        glUseProgram(program ? program->getProgram() : 0);
        m_pProgram = program;
    }
}
// 0x004e1550: filter table indexed by Material::TextureFilter, textures without mip levels
// fall back to plain nearest/linear; wrap table indexed by Material::TextureAddress.
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
        if (sm_maxAnisotropy > 0.0f)
            glTexParameterf(texture->getTarget(), GL_TEXTURE_MAX_ANISOTROPY_EXT,
                            filter == Material::Anisotropic ? sm_maxAnisotropy : 1.0f);
    }
    if (address >= 0)
    {
        // 0x00617368: {s, t, r} per address mode
        static constexpr GLint wraps[6][3] = {
            {GL_REPEAT, GL_REPEAT, GL_REPEAT},
            {GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE},
            {GL_REPEAT, GL_CLAMP_TO_EDGE, GL_REPEAT},
            {GL_CLAMP_TO_EDGE, GL_REPEAT, GL_REPEAT},
            {GL_REPEAT, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE},
            {GL_CLAMP_TO_EDGE, GL_REPEAT, GL_CLAMP_TO_EDGE}};
        glTexParameteri(texture->getTarget(), GL_TEXTURE_WRAP_S, wraps[address][0]);
        glTexParameteri(texture->getTarget(), GL_TEXTURE_WRAP_T, wraps[address][1]);
        glTexParameteri(texture->getTarget(), GL_TEXTURE_WRAP_R, wraps[address][2]);
    }
}
// 0x004e1630
void RenderContextGL::setUniformTexture(const char* name, TextureGL* texture, int filter,
                                        int address, int unit)
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(texture->getTarget(), texture->getHandle());
    GLint location = glGetUniformLocation(m_pProgram->getProgram(), name);
    glUniform1i(location, unit);
    setTextureFilterAndWrapMode(texture, filter, address);
}
void RenderContextGL::setUniformTexture(int uniform, TextureGL* texture, int filter, int address,
                                        int unit)
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(texture->getTarget(), texture->getHandle());
    glUniform1i(m_pProgram->getUniform(uniform), unit);
    setTextureFilterAndWrapMode(texture, filter, address);
}
// 0x004e1a80
void RenderContextGL::setShaderParams(const Material& material, int firstTextureUnit)
{
    int unit = firstTextureUnit;
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
                setUniformTexture(p.name.c_str(), textureGL, p.filter, p.address, unit);
                ++unit;
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
// 0x004e1a10: skinning matrices (model space) uploaded as 3 vec4 rows per bone.
void RenderContextGL::setSkinningMatrices(const MeshEntity& entity)
{
    static float skinningMatrices[64 * 12];
    entity.getSkeleton()->computeSkinningMatrices(entity, skinningMatrices, m_frameCounter);
    int numBones = entity.getSkeleton()->getBoneCount();
    if (numBones > 64)
        numBones = 64;
    m_pProgram->setUniform4v(ShaderProgramGL::U_skinningMatrices, skinningMatrices, numBones * 3);
}
// 0x004e1430
void* RenderContextGL::streamWrite(int bytes)
{
    glBindBuffer(GL_ARRAY_BUFFER, m_streamBuffer);
    if (m_streamOffset + bytes > StreamBufferSize)
    {
        // orphan the buffer and start over
        glBufferData(GL_ARRAY_BUFFER, StreamBufferSize, 0, GL_STREAM_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, m_streamBuffer);
        m_streamOffset = 0;
    }
    void* data = glMapBufferRange(GL_ARRAY_BUFFER, m_streamOffset, bytes,
                                  GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
    if (!data)
        return 0;
    m_streamOffset += bytes;
    return data;
}
// 0x004e14b0: count is vertices, except for quads where it is the number of quads * 4
// drawn through the shared index buffer.
void RenderContextGL::streamDrawPrimitive(int primitive, int count)
{
    glUnmapBuffer(GL_ARRAY_BUFFER);
    if (primitive == Prim_Quads)
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_quadIndexBuffer);
        glDrawElements(GL_TRIANGLES, (count / 4) * 6, GL_UNSIGNED_SHORT, 0);
        return;
    }
    static constexpr GLenum modes[3] = {GL_POINTS, GL_LINES, GL_TRIANGLES};
    glDrawArrays(modes[primitive], 0, count);
}
// 0x004e1c70
GLuint RenderContextGL::compileShader(const char* source, GLenum type, const char* const* defines,
                                      String& log)
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
        return 0;
    }
    return shader;
}

constexpr int MaxShaderIncludes = 100;
constexpr int IncludeDirectiveLength = 10; // #include "

// 0x004e20e0
void RenderContextGL::preprocessShader(const char* filename, const char* shaderDir, String& out,
                                       int& includeCount)
{
    if (includeCount > MaxShaderIncludes)
        throw Exception("too many include directives (infinite recursion?)");
    String directory = getPath(filename);
    int length = 0;
    char* data = readFile(filename, length);
    if (!data)
        throw Exception("Failed to load shader file %s", filename);
    String source(data);
    delete[] data;
    source.replace("\r", "");
    Array<String> lines = split(source.c_str(), "\n");
    int fileNumber = includeCount;
    for (int i = 0; i < lines.size(); ++i)
    {
        const String& line = lines[i];
        if (!line.startsWith("#include"))
        {
            out.append(line.c_str());
            out.append("\n");
            continue;
        }
        // #include "name": next to the including file, otherwise in the shader directory
        // (substr takes inclusive positions; the last character is the closing quote)
        String name = line.substr(IncludeDirectiveLength, line.size() - 2);
        while (name.size() > 0 && (name.endsWith("\"") || name.endsWith(" ")))
            name = name.substr(0, name.size() - 2);
        String path = formatString("%s/%s", directory.c_str(), name.c_str());
        if (!fileExists(path.c_str()))
            path = formatString("%s/%s", shaderDir, name.c_str());
        ++includeCount;
        out.append(formatString("#line %d %d\n", 1, includeCount).c_str());
        preprocessShader(path.c_str(), shaderDir, out, includeCount);
        out.append(formatString("#line %d %d\n", i + 1, fileNumber).c_str());
    }
}
// 0x004e2470
GLuint RenderContextGL::compileShaderFromFile(const char* filename, GLenum type,
                                              const char* const* defines)
{
    debugPrint("Compiling shader %s\n", filename);
    String source;
    int includeCount = 0;
    preprocessShader(filename, "shaders/gl", source, includeCount);
    String log;
    GLuint shader = compileShader(source.c_str(), type, defines, log);
    if (!shader)
        throw Exception("Failed to compile shader %s:\n%s", filename, log.c_str());
    return shader;
}

} // namespace engine
