// Reconstructed from Grimrock.bin.x86 RendererGL.cpp, CommonResourcesGL.cpp and
// RenderContextSDL.cpp.
#include "engine/RendererGL.h"
#include "core/Exception.h"
#include "core/Image.h"
#include "core/Math.h"
#include "core/Sys.h"
#include "core/Window.h"
#include "engine/AssetProcessor.h"
#include "engine/Camera.h"
#include "engine/DDSLoader.h"
#include "engine/Font.h"
#include "engine/Graphics.h"
#include "engine/ImmediateMode.h"
#include "engine/ImmediateModeGL.h"
#include "engine/LightPrePassRendererGL.h"
#include "engine/NotebookRendererGL.h"
#include "engine/PostGL.h"
#include "engine/RenderEntity.h"
#include "engine/RotTexData.h"
#include "engine/Scene.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace engine
{

using namespace core;

constexpr int ReportedTextureMemory = 0x10000000; // 256 MB

// ---- RenderContextSDL ------------------------------------------------------------

// 0x0810ff90
RenderContextSDL::RenderContextSDL(Window* window, bool windowed, bool verticalSync)
    : m_pCoreWindow(window), m_backBuffer(0), m_backBufferColor(0), m_backBufferDepth(0),
      m_backBufferWidth(0), m_backBufferHeight(0)
{
    m_pWindow = window->getSDLWindow();
    m_context = SDL_GL_CreateContext(m_pWindow);
    SDL_GL_MakeCurrent(m_pWindow, m_context);
    SDL_GL_SetSwapInterval(verticalSync ? 1 : 0);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
    {
        sysMessageBox("glewInit failed", "Failed to initialize glew", MessageBox_Ok);
        exit(1);
    }
    glGetError(); // glewInit may leave an error behind on core profiles
    contextCreated();
    updateBackBuffer();
}
// The frame is always rendered into an off-screen buffer of the requested size and
// blitted onto the window at swap time; that keeps the picture correct while the window
// manager is still resizing a fullscreen window (the first frames after creation) and
// costs one copy per frame.
void RenderContextSDL::updateBackBuffer()
{
    int width = m_pCoreWindow->getWidth(), height = m_pCoreWindow->getHeight();
    if (!m_backBuffer || m_backBufferWidth != width || m_backBufferHeight != height)
    {
        destroyBackBuffer();
        createBackBuffer(width, height);
    }
}
// 0x0810ff40
RenderContextSDL::~RenderContextSDL()
{
    destroyBackBuffer();
    contextTerminating();
    SDL_GL_DestroyContext(m_context);
}
void RenderContextSDL::createBackBuffer(int width, int height)
{
    debugPrint("Frame buffer %dx%d\n", width, height);
    m_backBufferWidth = width;
    m_backBufferHeight = height;
    glGenRenderbuffers(1, &m_backBufferColor);
    glBindRenderbuffer(GL_RENDERBUFFER, m_backBufferColor);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
    glGenRenderbuffers(1, &m_backBufferDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, m_backBufferDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glGenFramebuffers(1, &m_backBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, m_backBuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                              m_backBufferColor);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              m_backBufferDepth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw Exception("Could not create the %dx%d back buffer", width, height);
    m_defaultFrameBuffer = m_backBuffer;
    checkGLErrors("createBackBuffer");
}
void RenderContextSDL::destroyBackBuffer()
{
    if (!m_backBuffer)
        return;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &m_backBuffer);
    glDeleteRenderbuffers(1, &m_backBufferColor);
    glDeleteRenderbuffers(1, &m_backBufferDepth);
    m_backBuffer = m_backBufferColor = m_backBufferDepth = 0;
    m_defaultFrameBuffer = 0;
}
// 0x0810fd30
void RenderContextSDL::swapBuffers()
{
    if (m_backBuffer)
    {
        // letterboxed, linearly filtered copy of the frame onto the window
        int x, y, width, height, windowWidth, windowHeight;
        m_pCoreWindow->getPresentationRect(x, y, width, height);
        SDL_GetWindowSizeInPixels(m_pWindow, &windowWidth, &windowHeight);
        // the presentation rectangle is in window coordinates, the blit in pixels
        float pixelDensity = SDL_GetWindowPixelDensity(m_pWindow);
        if (pixelDensity > 0.0f && pixelDensity != 1.0f)
        {
            x = (int)lrintf(x * pixelDensity);
            y = (int)lrintf(y * pixelDensity);
            width = (int)lrintf(width * pixelDensity);
            height = (int)lrintf(height * pixelDensity);
        }
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_backBuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glDisable(GL_SCISSOR_TEST);
        glViewport(0, 0, windowWidth, windowHeight);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        // the frame is stored top-down in GL terms like the window would be, so no flip
        glBlitFramebuffer(0, 0, m_backBufferWidth, m_backBufferHeight, x, windowHeight - y - height,
                          x + width, windowHeight - y, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, m_backBuffer);
        checkGLErrors("swapBuffers");
    }
    SDL_GL_SwapWindow(m_pWindow);
}
// 0x0810fee0
void RenderContextSDL::SetupGLAttributes()
{
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
}
// 0x0810fd50: display modes with a sane aspect ratio, not larger than the desktop.
void RenderContextSDL::enumerateResolutions(Array<std::pair<int, int>>& resolutions)
{
    int numModes = 0;
    SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(SDL_GetPrimaryDisplay(), &numModes);
    int desktopW, desktopH;
    sysGetDesktopDisplayMode(desktopW, desktopH);
    for (int i = 0; modes && i < numModes; ++i)
    {
        const SDL_DisplayMode& mode = *modes[i];
        if (mode.w > mode.h * 2 || mode.h > mode.w * 2 || mode.w > desktopW || mode.h > desktopH ||
            SDL_BITSPERPIXEL(mode.format) <= 14)
            continue;
        bool found = false;
        for (int k = 0; k < resolutions.size(); ++k)
            if (resolutions[k].first == mode.w && resolutions[k].second == mode.h)
                found = true;
        if (!found)
            resolutions.push_back(std::pair<int, int>(mode.w, mode.h));
    }
    SDL_free(modes);
}

// ---- RenderableTextureGL ---------------------------------------------------------

// 0x081104f0
RenderableTextureGL::RenderableTextureGL() : m_pTexture(0) {}
// 0x081104a0
RenderableTextureGL::~RenderableTextureGL()
{
    delete m_pTexture;
}
void RenderableTextureGL::setTexture(TextureGL* texture)
{
    if (texture != m_pTexture)
    {
        delete m_pTexture;
        m_pTexture = texture;
    }
}
// 0x08112fe0: BGRA image with generated mip maps.
void RenderableTextureGL::init(const Image& image)
{
    int w = image.getWidth(), h = image.getHeight();
    int levels = 1;
    for (int s = (w > h ? w : h) >> 1; s; s >>= 1)
        ++levels;
    Texture2DGL* t =
        new Texture2DGL(w, h, levels, GL_RGBA8, GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_REPEAT);
    setTexture(t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, image.getData());
    checkGLErrors("glTexImage2D");
    glGenerateMipmap(GL_TEXTURE_2D);
    checkGLErrors("glGenerateMipmap");
}
// 0x08112760
void RenderableTextureGL::load(const char* filename, int skipMipLevels, bool srgb)
{
    AssetProcessor* processor = findAssetProcessor(AssetProcessor::TextureAsset, filename);
    processor->processFile(filename);
    String native = processor->getNativeFile(filename);
    m_filename = filename;
    DDSLoader dds(native.c_str());
    int mipLevels = dds.hasMipMaps() ? dds.getMipMapCount() : 0;
    GLenum format;
    bool swapRedBlue;
    switch (dds.getFormat())
    {
    case DDSLoader::FormatDXT1:
        format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
        swapRedBlue = false;
        break;
    case DDSLoader::FormatDXT3:
        format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
        swapRedBlue = false;
        break;
    case DDSLoader::FormatDXT5:
        format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        swapRedBlue = false;
        break;
    case DDSLoader::FormatA8R8G8B8:
        format = GL_RGBA;
        swapRedBlue = true;
        break;
    case DDSLoader::FormatX8R8G8B8:
        format = GL_RGB;
        swapRedBlue = true;
        break;
    default:
        throw Exception("Unsupported texture format in file %s", filename);
    }
    if (srgb)
    {
        switch (format)
        {
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
            format = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT;
            break;
        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
            format = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT;
            break;
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
            format = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
            break;
        case GL_RGBA:
            format = GL_SRGB_ALPHA;
            swapRedBlue = false;
            break;
        case GL_RGB:
            format = GL_SRGB;
            swapRedBlue = true;
            break;
        }
    }
    int width = dds.getWidth(), height = dds.getHeight();
    // Only drop mip levels of big enough textures.
    if (skipMipLevels > 0 && dds.getType() == DDSLoader::Texture2D)
    {
        if (width < 64 || height < 64 || mipLevels < 3)
        {
            skipMipLevels = 0;
        }
        else
        {
            for (int i = 0; i < skipMipLevels; ++i)
            {
                width /= 2;
                height /= 2;
            }
            mipLevels -= skipMipLevels;
        }
    }
    Texture2DGL* t =
        new Texture2DGL(width, height, mipLevels, format,
                        mipLevels < 2 ? GL_LINEAR : GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR, GL_REPEAT);
    setTexture(t);
    if (dds.getType() != DDSLoader::Texture2D)
        throw Exception("Unsupported dds texture type");
    for (int level = 0; level < mipLevels; ++level)
    {
        int size = dds.getSurfaceSize(level + skipMipLevels);
        unsigned char* data = new unsigned char[size];
        dds.loadSurface(level + skipMipLevels, data);
        if (swapRedBlue || format == GL_SRGB_ALPHA)
        {
            for (int i = 0; i < width * height; ++i)
            {
                unsigned char tmp = data[i * 4];
                data[i * 4] = data[i * 4 + 2];
                data[i * 4 + 2] = tmp;
            }
            glTexImage2D(GL_TEXTURE_2D, level, format, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         data);
            checkGLErrors("glTexImage2D");
        }
        else
        {
            glCompressedTexImage2D(GL_TEXTURE_2D, level, format, width, height, 0, size, data);
            checkGLErrors("glCompressedTexImage2D");
        }
        delete[] data;
        width = width / 2 > 0 ? width / 2 : 1;
        height = height / 2 > 0 ? height / 2 : 1;
    }
}
// 0x08114680
void RenderableTextureGL::reload() {}
// 0x081103f0
void RenderableTextureGL::save(const char* filename) {}
// 0x08114690 / 0x081146a0
int RenderableTextureGL::getWidth() const
{
    return m_pTexture ? m_pTexture->getWidth() : 0;
}
int RenderableTextureGL::getHeight() const
{
    return m_pTexture ? m_pTexture->getHeight() : 0;
}

// ---- RenderableShaderGL ----------------------------------------------------------

RenderableShaderGL::RenderableShaderGL()
{
    memset(m_programs, 0, sizeof(m_programs));
}
// 0x08114a80
RenderableShaderGL::~RenderableShaderGL()
{
    for (int p = 0; p < NumPasses; ++p)
        for (int v = 0; v < NumVariants; ++v)
            delete m_programs[p][v];
}
static void setProgram(ShaderProgramGL*& slot, ShaderProgramGL* program)
{
    if (slot != program)
    {
        delete slot;
        slot = program;
    }
}
// 0x08110b20
void RenderableShaderGL::initSurfaceShader(const char* vertexShader, const char* geometryShader,
                                           const char* materialShader, const char* unlitShader,
                                           const char* shadowShader)
{
    if (!vertexShader)
        vertexShader = "shaders/gl/Mesh.vsh";
    if (!geometryShader)
        geometryShader = "shaders/gl/MeshGeometry.fsh";
    if (!materialShader)
        materialShader = "shaders/gl/MeshMaterial.fsh";
    if (!unlitShader)
        unlitShader = "shaders/gl/MeshUnlit.fsh";
    if (!shadowShader)
        shadowShader = "shaders/gl/MeshShadow.fsh";
    static constexpr const char* defSkin[] = {"SKINNING", 0};
    static constexpr const char* defNormal[] = {"NORMAL_MAP", 0};
    static constexpr const char* defNormalSkin[] = {"NORMAL_MAP", "SKINNING", 0};
    static constexpr const char* defAlpha[] = {"ALPHA_TEST", 0};
    static constexpr const char* defNormalAlpha[] = {"NORMAL_MAP", "ALPHA_TEST", 0};
    GLuint vs = RenderContextGL::compileShaderFromFile(vertexShader, GL_VERTEX_SHADER, 0);
    GLuint vsSkin = RenderContextGL::compileShaderFromFile(vertexShader, GL_VERTEX_SHADER, defSkin);
    GLuint vsNormal =
        RenderContextGL::compileShaderFromFile(vertexShader, GL_VERTEX_SHADER, defNormal);
    GLuint vsNormalSkin =
        RenderContextGL::compileShaderFromFile(vertexShader, GL_VERTEX_SHADER, defNormalSkin);
    GLuint fsGeom = RenderContextGL::compileShaderFromFile(geometryShader, GL_FRAGMENT_SHADER, 0);
    GLuint fsGeomNormal =
        RenderContextGL::compileShaderFromFile(geometryShader, GL_FRAGMENT_SHADER, defNormal);
    GLuint fsGeomAlpha =
        RenderContextGL::compileShaderFromFile(geometryShader, GL_FRAGMENT_SHADER, defAlpha);
    GLuint fsGeomNormalAlpha =
        RenderContextGL::compileShaderFromFile(geometryShader, GL_FRAGMENT_SHADER, defNormalAlpha);
    GLuint fsMaterial =
        RenderContextGL::compileShaderFromFile(materialShader, GL_FRAGMENT_SHADER, 0);
    GLuint fsUnlit = RenderContextGL::compileShaderFromFile(unlitShader, GL_FRAGMENT_SHADER, 0);
    GLuint fsShadow = RenderContextGL::compileShaderFromFile(shadowShader, GL_FRAGMENT_SHADER, 0);
    GLuint fsShadowAlpha =
        RenderContextGL::compileShaderFromFile(shadowShader, GL_FRAGMENT_SHADER, defAlpha);

    setProgram(m_programs[GeometryPass][0], new ShaderProgramGL(vs, fsGeom));
    setProgram(m_programs[GeometryPass][Skinning], new ShaderProgramGL(vsSkin, fsGeom));
    setProgram(m_programs[GeometryPass][NormalMap], new ShaderProgramGL(vsNormal, fsGeomNormal));
    setProgram(m_programs[GeometryPass][NormalMap | Skinning],
               new ShaderProgramGL(vsNormalSkin, fsGeomNormal));
    setProgram(m_programs[GeometryPass][AlphaTest], new ShaderProgramGL(vs, fsGeomAlpha));
    setProgram(m_programs[GeometryPass][AlphaTest | Skinning],
               new ShaderProgramGL(vsSkin, fsGeomAlpha));
    setProgram(m_programs[GeometryPass][NormalMap | AlphaTest],
               new ShaderProgramGL(vsNormal, fsGeomNormalAlpha));
    setProgram(m_programs[GeometryPass][NormalMap | AlphaTest | Skinning],
               new ShaderProgramGL(vsNormalSkin, fsGeomNormalAlpha));
    setProgram(m_programs[MaterialPass][0], new ShaderProgramGL(vs, fsMaterial));
    setProgram(m_programs[MaterialPass][Skinning], new ShaderProgramGL(vsSkin, fsMaterial));
    setProgram(m_programs[UnlitPass][0], new ShaderProgramGL(vs, fsUnlit));
    setProgram(m_programs[UnlitPass][Skinning], new ShaderProgramGL(vsSkin, fsUnlit));
    setProgram(m_programs[ShadowPass][0], new ShaderProgramGL(vs, fsShadow));
    setProgram(m_programs[ShadowPass][Skinning], new ShaderProgramGL(vsSkin, fsShadow));
    setProgram(m_programs[ShadowPass][AlphaTest], new ShaderProgramGL(vs, fsShadowAlpha));
    setProgram(m_programs[ShadowPass][AlphaTest | Skinning],
               new ShaderProgramGL(vsSkin, fsShadowAlpha));
    GLuint shaders[] = {vs,         vsSkin,       vsNormal,    vsNormalSkin,
                        fsGeom,     fsGeomNormal, fsGeomAlpha, fsGeomNormalAlpha,
                        fsMaterial, fsUnlit,      fsShadow,    fsShadowAlpha};
    for (size_t i = 0; i < sizeof(shaders) / sizeof(shaders[0]); ++i)
        glDeleteShader(shaders[i]);
}
// 0x08110ab0
void RenderableShaderGL::initPostProcessShader(const char* fragmentShader)
{
    setProgram(m_programs[0][0], new ShaderProgramGL("shaders/gl/PostProcess.vsh", fragmentShader));
}

// ---- RenderableMeshGL ------------------------------------------------------------

// 0x08110570
RenderableMeshGL::RenderableMeshGL()
    : m_vertexBuffer(0), m_indexBuffer(0), m_indexSize(0), m_numVertices(0), m_stride(0),
      m_normalOffset(0), m_tangentOffset(0), m_bitangentOffset(0), m_texcoordOffset(0),
      m_boneIndicesOffset(0), m_boneWeightsOffset(0), m_skinned(false)
{
}
// 0x08111550
RenderableMeshGL::~RenderableMeshGL()
{
    if (m_vertexBuffer)
        glDeleteBuffers(1, &m_vertexBuffer);
    if (m_indexBuffer)
        glDeleteBuffers(1, &m_indexBuffer);
}
// 0x081117e0: interleaves the vertex arrays into one static buffer; bone indices become
// bytes and weights are quantised to bytes summing to 255.
void RenderableMeshGL::init(Mesh& mesh, bool keepSourceData)
{
    if (m_vertexBuffer)
        glDeleteBuffers(1, &m_vertexBuffer);
    if (m_indexBuffer)
        glDeleteBuffers(1, &m_indexBuffer);
    const Vec3* pos = (const Vec3*)mesh.getVertexArray(Mesh::Position);
    const Vec3* nrm = (const Vec3*)mesh.getVertexArray(Mesh::Normal);
    const Vec3* tan = (const Vec3*)mesh.getVertexArray(Mesh::Tangent);
    const Vec3* bit = (const Vec3*)mesh.getVertexArray(Mesh::Bitangent);
    const Vec2* uv = (const Vec2*)mesh.getVertexArray(Mesh::Texcoord0);
    const int* boneIdx = (const int*)mesh.getVertexArray(Mesh::BoneIndices);
    const float* boneW = (const float*)mesh.getVertexArray(Mesh::BoneWeights);
    int boneComponents = mesh.getVertexArrayInfo(Mesh::BoneIndices).components;
    m_skinned = boneIdx && boneW && boneComponents > 0;
    m_numVertices = mesh.getNumVertices();
    int stride = 12;
    m_normalOffset = m_tangentOffset = m_bitangentOffset = m_texcoordOffset = -1;
    m_boneIndicesOffset = m_boneWeightsOffset = -1;
    if (nrm)
    {
        m_normalOffset = stride;
        stride += 12;
    }
    if (tan)
    {
        m_tangentOffset = stride;
        stride += 12;
    }
    if (bit)
    {
        m_bitangentOffset = stride;
        stride += 12;
    }
    if (uv)
    {
        m_texcoordOffset = stride;
        stride += 8;
    }
    if (m_skinned)
    {
        m_boneIndicesOffset = stride;
        m_boneWeightsOffset = stride + 4;
        stride += 8;
    }
    m_stride = stride;
    glGenBuffers(1, &m_vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)m_numVertices * stride, 0, GL_STATIC_DRAW);
    char* out = (char*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    for (int i = 0; i < m_numVertices; ++i)
    {
        char* v = out + i * stride;
        memcpy(v, &pos[i], 12);
        if (nrm)
            memcpy(v + m_normalOffset, &nrm[i], 12);
        if (tan)
            memcpy(v + m_tangentOffset, &tan[i], 12);
        if (bit)
            memcpy(v + m_bitangentOffset, &bit[i], 12);
        if (uv)
            memcpy(v + m_texcoordOffset, &uv[i], 8);
        if (m_skinned)
        {
            unsigned char* bi = (unsigned char*)(v + m_boneIndicesOffset);
            unsigned char* bw = (unsigned char*)(v + m_boneWeightsOffset);
            int sum = 0;
            for (int c = 0; c < 4; ++c)
            {
                if (c < boneComponents)
                {
                    bi[c] = (unsigned char)boneIdx[i * boneComponents + c];
                    int w = (int)lrintf(boneW[i * boneComponents + c] * 255.0f);
                    bw[c] = (unsigned char)w;
                    sum += w & 0xff;
                }
                else
                {
                    bi[c] = 0;
                    bw[c] = 0;
                }
            }
            // keep the weights normalised after quantisation
            if (sum != 255 && boneComponents > 0)
                bw[0] = (unsigned char)(bw[0] + (255 - sum));
        }
    }
    glUnmapBuffer(GL_ARRAY_BUFFER);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    const Array<int>& indices = mesh.getIndices();
    m_indexSize = m_numVertices > 0xffff ? 4 : 2;
    glGenBuffers(1, &m_indexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)indices.size() * m_indexSize, 0,
                 GL_STATIC_DRAW);
    void* idx = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
    if (m_indexSize == 2)
    {
        unsigned short* p = (unsigned short*)idx;
        for (int i = 0; i < indices.size(); ++i)
            p[i] = (unsigned short)indices[i];
    }
    else
    {
        memcpy(idx, indices.data(), indices.size() * 4);
    }
    glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    m_segments.clear();
    for (int i = 0; i < mesh.getNumSegments(); ++i)
    {
        const MeshSegment& s = mesh.getSegment(i);
        MeshSegment seg = s;
        seg.primitiveType =
            s.primitiveType == 2 ? GL_TRIANGLES : (s.primitiveType == 1 ? GL_LINES : GL_POINTS);
        m_segments.push_back(seg);
    }
    m_bounds = mesh.getBoundingBox();
    m_sourceData.reset(keepSourceData ? new Mesh(mesh) : 0);
}
// 0x08114660
Mesh* RenderableMeshGL::getSourceData()
{
    return m_sourceData.get();
}
// 0x08114670
const AABox3& RenderableMeshGL::getBoundingBox() const
{
    return m_bounds;
}
// 0x081121f0
Array<SharedPtr<Material>> RenderableMeshGL::getMaterials() const
{
    Array<SharedPtr<Material>> result;
    result.reserve(m_segments.size());
    for (int i = 0; i < m_segments.size(); ++i)
        result.push_back(m_segments[i].material);
    return result;
}
// 0x08110120
void RenderableMeshGL::activate()
{
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, m_stride, 0);
    const int offsets[4] = {m_normalOffset, m_tangentOffset, m_bitangentOffset, m_texcoordOffset};
    for (int a = 0; a < 4; ++a)
    {
        if (offsets[a] < 0)
        {
            glDisableVertexAttribArray(a + 1);
        }
        else
        {
            glEnableVertexAttribArray(a + 1);
            glVertexAttribPointer(a + 1, a == 3 ? 2 : 3, GL_FLOAT, GL_FALSE, m_stride,
                                  (const void*)(intptr_t)offsets[a]);
        }
    }
    if (!m_skinned)
    {
        glDisableVertexAttribArray(6);
        glDisableVertexAttribArray(7);
    }
    else
    {
        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 4, GL_UNSIGNED_BYTE, GL_FALSE, m_stride,
                              (const void*)(intptr_t)m_boneIndicesOffset);
        glEnableVertexAttribArray(7);
        glVertexAttribPointer(7, 4, GL_UNSIGNED_BYTE, GL_FALSE, m_stride,
                              (const void*)(intptr_t)m_boneWeightsOffset);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
}
// 0x08110520
void RenderableMeshGL::renderSegment(int segment)
{
    const MeshSegment& s = m_segments[segment];
    glDrawElements(s.primitiveType, s.numTriangles * 3,
                   m_indexSize == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
                   (const void*)(intptr_t)(m_indexSize * s.firstIndex));
}
// 0x081103d0 / 0x081103e0
void RenderableMeshGL::drawNormals(const Matrix4x3& m) {}
void RenderableMeshGL::drawTangents(const Matrix4x3& m) {}

// ---- CommonResourcesGL -----------------------------------------------------------

RenderableTextureGL* CommonResourcesGL::WhiteMap = 0;
RenderableTextureGL* CommonResourcesGL::DefaultNormalMap = 0;
RenderableTextureGL* CommonResourcesGL::RotMap = 0;
RenderableMeshGL* CommonResourcesGL::SphereMesh = 0;
RenderableMeshGL* CommonResourcesGL::ConeMesh = 0;
Font* CommonResourcesGL::DefaultFont = 0;
RenderableShaderGL* CommonResourcesGL::BlitShader = 0;
Material* CommonResourcesGL::BlitMaterial = 0;

// 0x081154e0
CommonResourcesGL::CommonResourcesGL()
{
    {
        Image white(1, 1);
        white.setPixel(0, 0, Color::White);
        SharedPtr<RenderableTextureGL> t(new RenderableTextureGL);
        t->init(white);
        m_textures.push_back(t);
        WhiteMap = t.get();
    }
    {
        Image flat(1, 1);
        flat.setPixel(0, 0, Color(128, 128, 255, 255));
        SharedPtr<RenderableTextureGL> t(new RenderableTextureGL);
        t->init(flat);
        m_textures.push_back(t);
        DefaultNormalMap = t.get();
    }
    {
        // RotTexSize^2 rotation vectors (cos, -sin, sin, cos) for the ambient occlusion kernel
        constexpr int RotTexSize = 32;
        Image rot(RotTexSize, RotTexSize);
        for (int i = 0; i < RotTexSize * RotTexSize; ++i)
        {
            float angle = (float)g_rotTex[i] * PI * 2.0f / 255.0f;
            float cosA = std::cos(angle), sinA = std::sin(angle);
            unsigned char cosByte = (unsigned char)lrintf((cosA * 0.5f + 0.5f) * 255.0f);
            rot.setPixel(i % RotTexSize, i / RotTexSize,
                         Color(cosByte, (unsigned char)lrintf((-sinA * 0.5f + 0.5f) * 255.0f),
                               (unsigned char)lrintf((sinA * 0.5f + 0.5f) * 255.0f), cosByte));
        }
        SharedPtr<RenderableTextureGL> rotTexture(new RenderableTextureGL);
        rotTexture->init(rot);
        m_textures.push_back(rotTexture);
        RotMap = rotTexture.get();
    }
    {
        SharedPtr<Mesh> sphere(createSphere(2));
        SharedPtr<RenderableMeshGL> m(new RenderableMeshGL);
        m->init(*sphere, false);
        m_meshes.push_back(m);
        SphereMesh = m.get();
        SharedPtr<Mesh> cone(createCone(10));
        SharedPtr<RenderableMeshGL> c(new RenderableMeshGL);
        c->init(*cone, false);
        m_meshes.push_back(c);
        ConeMesh = c.get();
    }
    {
        SharedPtr<Font> font(new Font(Font::Fixedsys));
        m_fonts.push_back(font);
        DefaultFont = font.get();
    }
    {
        SharedPtr<RenderableShader> shader(new RenderableShaderGL);
        ((RenderableShaderGL*)shader.get())->initPostProcessShader("shaders/gl/PostProcess.fsh");
        m_shaders.push_back(shader);
        BlitShader = (RenderableShaderGL*)shader.get();
        SharedPtr<Material> material(new Material(""));
        material->setShader(BlitShader);
        m_materials.push_back(material);
        BlitMaterial = material.get();
    }
}
// 0x08114f10
CommonResourcesGL::~CommonResourcesGL()
{
    WhiteMap = 0;
    DefaultNormalMap = 0;
    RotMap = 0;
    SphereMesh = 0;
    ConeMesh = 0;
    DefaultFont = 0;
    BlitShader = 0;
    BlitMaterial = 0;
}

// ---- RendererGL ------------------------------------------------------------------

// 0x08110920
RendererGL::RendererGL()
    : m_pContext(0), m_pGraphics(0), m_pCommonResources(0), m_pRenderVisitor(0),
      m_pImmediateMode(0), m_pNotebookRenderer(0), m_pLightPrePassRenderer(0),
      m_pAmbientOcclusion(0), m_pFogFilter(0), m_pTonemapper(0)
{
    sm_pActiveRenderer = this;
}
// 0x08114250
RendererGL::~RendererGL()
{
    RenderBuffer::freeAllTemporaries();
    delete m_pTonemapper;
    delete m_pFogFilter;
    delete m_pAmbientOcclusion;
    delete m_pLightPrePassRenderer;
    delete m_pNotebookRenderer;
    delete m_pImmediateMode;
    delete m_pRenderVisitor;
    delete m_pCommonResources;
    delete m_pGraphics;
    delete m_pContext;
}
// 0x08113310
void RendererGL::init(const RendererConfig& config)
{
    m_config = config;
    m_viewportX = 0;
    m_viewportY = 0;
    m_viewportWidth = config.width;
    m_viewportHeight = config.height;
    m_pContext = new RenderContextSDL(config.window, !config.windowed, config.verticalSync != 0);
    m_pGraphics = new GraphicsGL(m_pContext);
    m_pRenderVisitor = new RenderVisitor;
    m_pCommonResources = new CommonResourcesGL;
    m_pImmediateMode = new ImmediateModeGL(m_pContext);
    if (!config.notebookMode)
    {
        m_pLightPrePassRenderer =
            new LightPrePassRendererGL(m_pContext, config.width, config.height);
        m_pAmbientOcclusion =
            new ScreenSpaceAmbientOcclusionGL(m_pContext, config.width, config.height);
        m_pFogFilter = new FogFilterGL(m_pContext);
        m_pTonemapper = new TonemapperGL(m_pContext);
    }
    else
    {
        m_pNotebookRenderer = new NotebookRendererGL(m_pContext);
    }
    registerAssetProcessor(AssetProcessor::TextureAsset, "tga", "dds");
    registerAssetProcessor(AssetProcessor::TextureAsset, "dds", "dds");
    Graphics::sm_pActive = m_pGraphics;
    glEnable(GL_TEXTURE_2D);
    glFrontFace(GL_CW);
    checkGLErrors("glFrontFace");
}
// 0x081111d0
RendererInfo RendererGL::getRendererInfo()
{
    RendererInfo info;
    const char* vendor = (const char*)glGetString(GL_VENDOR);
    const char* renderer = (const char*)glGetString(GL_RENDERER);
    if (vendor)
        info.renderer = vendor;
    if (renderer)
        info.vendor = renderer;
    return info;
}
// 0x081108a0
void RendererGL::resizeRenderBuffers(int width, int height)
{
    m_config.width = width;
    m_config.height = height;
    m_pContext->resizeWindow(width, height);
    if (m_config.notebookMode)
        return;
    m_pLightPrePassRenderer->resizeRenderBuffers(width, height);
    m_pAmbientOcclusion->resizeRenderBuffers(width, height);
}
// 0x081100c0
bool RendererGL::isReadyToRender()
{
    return true;
}
// 0x081100d0
void RendererGL::setRenderWindow(RenderWindow* window) {}
// 0x08113900
void RendererGL::beginRender()
{
    m_pContext->updateBackBuffer();
    m_pContext->setRenderTarget(RenderContextGL::DefaultFrameBuffer);
    glViewport(0, 0, m_config.width, m_config.height);
    checkGLErrors("glViewport");
    glDepthMask(GL_TRUE);
    glClearColor(m_clearColor.r / 255.0f, m_clearColor.g / 255.0f, m_clearColor.b / 255.0f, 0.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    g_renderStats.visibleMeshes = 0;
    g_renderStats.visibleLights = 0;
    g_renderStats.visibleParticles = 0;
    g_renderStats.drawSegments = 0;
    g_renderStats.shadowSegments = 0;
    im::prepare(m_config.width, m_config.height);
}
// 0x08113ab0
void RendererGL::renderScene(Scene& scene, Camera& camera, RenderableTexture* target)
{
    m_pRenderVisitor->gatherEntities(scene, camera);
    static const bool debugLights = getenv("GRIMROCK_DEBUG_LIGHTS") != 0;
    if (debugLights)
    {
        static int frame = 0;
        if (++frame % 120 == 0)
        {
            const Vec3& cam = camera.getLocalToWorldMatrix().pos;
            debugPrint("--- lights, camera at %.2f %.2f %.2f\n", cam.x, cam.y, cam.z);
            const Array<Node*>& nodes = scene.getNodes();
            for (int i = 0; i < nodes.size(); ++i)
            {
                LightEntity* l = nodes[i]->getLightEntity();
                if (!l)
                    continue;
                bool visible = false;
                for (int k = 0; k < m_pRenderVisitor->m_lights.size(); ++k)
                    if (m_pRenderVisitor->m_lights[k] == l)
                        visible = true;
                const Vec3& p = nodes[i]->getLocalToWorldMatrix().pos;
                AABox3 b = l->getWorldBounds();
                debugPrint("  light type %d pos %.2f %.2f %.2f range %.2f color %.2f %.2f %.2f "
                           "hidden %d shadow %d vis %d bounds [%.1f %.1f %.1f]-[%.1f %.1f %.1f]\n",
                           l->getLightType(), p.x, p.y, p.z, l->getLightRange(),
                           l->getLightColor().x, l->getLightColor().y, l->getLightColor().z,
                           l->getHidden(), l->getCastShadow(), visible, b.min.x, b.min.y, b.min.z,
                           b.max.x, b.max.y, b.max.z);
            }
        }
    }
    if (!m_config.notebookMode)
    {
        LightPrePassRendererGL* lpp = m_pLightPrePassRenderer;
        lpp->setViewport(m_viewportX, m_viewportY, m_viewportWidth, m_viewportHeight);
        lpp->m_diffuseMapping = m_diffuseMapping;
        lpp->m_normalMapping = m_normalMapping;
        lpp->m_textureFilter = m_textureFilter;
        lpp->m_renderMeshes = m_renderMeshes;
        lpp->m_renderShadows = m_renderShadows;
        lpp->m_shadowQuality = m_shadowQuality;
        lpp->renderGeometryPass(camera, *m_pRenderVisitor);
        lpp->renderLightPass(camera, *m_pRenderVisitor);
        lpp->renderMaterialPass(camera, *m_pRenderVisitor);
        if (m_ambientOcclusion)
            m_pAmbientOcclusion->render(camera, lpp->getGeometryBuffer(), lpp->getColorBuffer(),
                                        m_ssaoQuality);
        if (m_fog)
        {
            m_pFogFilter->m_color = m_fogColor;
            m_pFogFilter->m_start = m_fogStart;
            m_pFogFilter->m_end = m_fogEnd;
            m_pFogFilter->render(lpp->getGeometryBuffer(), lpp->getColorBuffer());
        }
        lpp->renderTransparentPass(camera, *m_pRenderVisitor);
        Texture2DGL* targetTexture =
            target ? (Texture2DGL*)((RenderableTextureGL*)target)->getTexture() : 0;
        m_pTonemapper->render(lpp->getColorBuffer(), targetTexture);
        glViewport(0, 0, m_config.width, m_config.height);
        checkGLErrors("glViewport");
        glScissor(0, 0, m_config.width, m_config.height);
        checkGLErrors("glScissor");
        if (m_drawNormalBuffer)
            drawBuffer(lpp->getGeometryBuffer(), 1.0f);
        else if (m_drawGlossinessBuffer)
            drawBuffer(lpp->getGlossinessBuffer(), 0.01f);
        else if (m_drawLightBuffer)
            drawBuffer(lpp->getLightBuffer(), 1.0f);
        else if (m_drawAmbientOcclusionBuffer)
            drawBuffer(m_pAmbientOcclusion->getResultBuffer(), 1.0f);
        im::flushDebugDraw(camera.getViewProjectionMatrix(), CommonResourcesGL::DefaultFont);
        checkGLErrors("renderScene");
    }
    else
    {
        glViewport(m_viewportX, m_config.height - m_viewportY - m_viewportHeight, m_viewportWidth,
                   m_viewportHeight);
        checkGLErrors("glViewport");
        m_pNotebookRenderer->m_diffuseMapping = m_diffuseMapping;
        m_pNotebookRenderer->m_normalMapping = m_normalMapping;
        m_pNotebookRenderer->renderOpaquePass(camera, *m_pRenderVisitor);
        m_pNotebookRenderer->renderTransparentPass(camera, *m_pRenderVisitor);
        glViewport(0, 0, m_config.width, m_config.height);
        checkGLErrors("glViewport");
        im::flushDebugDraw(camera.getViewProjectionMatrix(), CommonResourcesGL::DefaultFont);
        checkGLErrors("renderScene");
    }
}
// 0x08110a20
RenderableTexture* RendererGL::renderStaticShadowMap(LightEntity& light, int size, float bias,
                                                     int flags)
{
    if (m_config.notebookMode)
        return 0;
    TextureCubeGL* cube = m_pLightPrePassRenderer->renderStaticShadowMap(light, size);
    RenderableTextureGL* texture = new RenderableTextureGL;
    texture->setTexture(cube);
    return texture;
}
// 0x081100e0
void RendererGL::endRender()
{
    // Debugging aid: with GRIMROCK_CAPTURE_DIR set, a file named "take" in that directory
    // makes the next frame's back buffer land there as capture.png (compositor independent).
    static const char* captureDir = getenv("GRIMROCK_CAPTURE_DIR");
    if (captureDir)
    {
        String trigger = String(captureDir) + "/take";
        if (sysFileExists(trigger.c_str()))
        {
            int w = m_config.width, h = m_config.height;
            Image shot(w, h);
            m_pContext->setRenderTarget(RenderContextGL::DefaultFrameBuffer);
            glReadBuffer(m_pContext->getDefaultFrameBuffer() ? GL_COLOR_ATTACHMENT0 : GL_BACK);
            glReadPixels(0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE, shot.getData());
            shot.flipY();
            shot.save((String(captureDir) + "/capture.png").c_str(), false);
            remove(trigger.c_str());
        }
    }
    m_pContext->swapBuffers();
}
// 0x08110100
void RendererGL::saveScreenShot(const char* filename) {}
// 0x08110620
RenderableMesh* RendererGL::createRenderableMesh()
{
    return new RenderableMeshGL;
}
// 0x081109d0
RenderableTexture* RendererGL::createRenderableTexture()
{
    return new RenderableTextureGL;
}
// 0x08110450
RenderableShader* RendererGL::createRenderableShader()
{
    return new RenderableShaderGL;
}
// 0x08110430
RenderWindow* RendererGL::createRenderWindow(const Window& window)
{
    return new RenderWindowGL;
}
// 0x081125d0: half float RGBA target with nearest filtering.
RenderableTexture* RendererGL::createRenderBuffer(int width, int height)
{
    RenderableTextureGL* texture = new RenderableTextureGL;
    texture->setTexture(
        new Texture2DGL(width, height, 1, GL_RGBA16F, GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE));
    return texture;
}
// 0x081112c0
void RendererGL::enumerateResolutions(Array<std::pair<int, int>>& resolutions)
{
    RenderContextSDL::enumerateResolutions(resolutions);
    std::sort(resolutions.begin(), resolutions.end());
}
// 0x08110110: a constant 256 MB; the game picks its texture resolution from it.
int RendererGL::getAvailableTextureMemory()
{
    return ReportedTextureMemory;
}
// 0x08110660
void RendererGL::drawBuffer(Texture2DGL* texture, float scale)
{
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    m_pContext->setBlendMode(RenderContextGL::Blend_Opaque);
    m_pContext->useProgram(0);
    for (int i = 0; i < 10; ++i)
        glDisableVertexAttribArray(i);
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture->getHandle());
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glColor3f(scale, scale, scale);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 1);
    glVertex3f(-1, 1, 0);
    glTexCoord2f(1, 1);
    glVertex3f(1, 1, 0);
    glTexCoord2f(1, 0);
    glVertex3f(1, -1, 0);
    glTexCoord2f(0, 0);
    glVertex3f(-1, -1, 0);
    glEnd();
}

} // namespace engine
