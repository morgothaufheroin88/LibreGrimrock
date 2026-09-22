// Reconstructed from grimrock2.exe RendererGL.cpp, RenderContextGL.cpp (the WGL context and
// the GL resources) and CommonResourcesGL.cpp.
#include "engine/RendererGL.h"
#include "core/Exception.h"
#include "core/Image.h"
#include "core/Math.h"
#include "core/MersenneTwister.h"
#include "core/Profiler.h"
#include "core/Sys.h"
#include "core/Window.h"
#include "engine/AssetProcessor.h"
#include "engine/Camera.h"
#include "engine/DDSLoader.h"
#include "engine/Font.h"
#include "engine/Graphics.h"
#include "engine/ImmediateModeGL.h"
#include "engine/LightPrePassRendererGL.h"
#include "engine/NotebookRendererGL.h"
#include "engine/OcclusionCullingGL.h"
#include "engine/PostGL.h"
#include "engine/RenderEntity.h"
#include "engine/RotTexData.h"
#include "engine/Scene.h"
#include "engine/VPXPlayerGL.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace engine
{

// Debugging aid: GRIMROCK_DEBUG_NOSHADOWS=1 renders the lights without their shadow
// maps; the scripts set the shadows from the configuration, so the switch is applied
// where the renderer hands its settings to the light pre-pass renderer.
static const bool g_debugNoShadows = getenv("GRIMROCK_DEBUG_NOSHADOWS") != 0;

using namespace core;

constexpr int ReportedTextureMemory = 0x10000000; // 256 MB

// ---- RenderContextSDL ------------------------------------------------------------

// 0x004e0cc0 (RenderContextWGL in the original)
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
// 0x004e1150
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
// 0x004e0ca0
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
void RenderContextSDL::SetupGLAttributes()
{
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
}
// 0x004e11f0: display modes with a sane aspect ratio, not larger than the desktop.
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

// 0x004e35b0
RenderableTextureGL::RenderableTextureGL() : m_pTexture(0) {}
// 0x004e3ed0
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
// 0x004e35f0: BGRA image with generated mip maps.
void RenderableTextureGL::init(const Image& image)
{
    int w = image.getWidth(), h = image.getHeight();
    int levels = 1;
    for (int s = (w > h ? w : h) >> 1; s; s >>= 1)
        ++levels;
    Texture2DGL* t = new Texture2DGL(w, h, levels, GL_RGBA8, GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR,
                                     GL_REPEAT, GL_RGBA);
    setTexture(t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, image.getData());
    checkGLErrors("glTexImage2D");
    glGenerateMipmap(GL_TEXTURE_2D);
    checkGLErrors("glGenerateMipmap");
}
// The GL format of a DDS surface format, in its sRGB variant when the texture holds
// colours; the uncompressed formats are stored as BGRA and swizzled on upload.
static GLenum getTextureFormat(int ddsFormat, bool srgb, const char* filename)
{
    switch (ddsFormat)
    {
    case DDSLoader::FormatDXT1:
        return srgb ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT : GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
    case DDSLoader::FormatDXT3:
        return srgb ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT : GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
    case DDSLoader::FormatDXT5:
        return srgb ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
    case DDSLoader::FormatA8R8G8B8:
        return srgb ? GL_SRGB_ALPHA : GL_RGBA;
    case DDSLoader::FormatX8R8G8B8:
        return srgb ? GL_SRGB : GL_RGB;
    case DDSLoader::FormatA16B16G16R16F:
        return GL_RGBA16F;
    default:
        throw Exception("Unsupported texture format (%d) in file %s", ddsFormat, filename);
    }
}
static bool isUncompressed(GLenum format)
{
    return format == GL_RGBA || format == GL_RGB || format == GL_SRGB || format == GL_SRGB_ALPHA;
}
// DDS stores 8 bit colour as BGRA
static void swapRedAndBlue(unsigned char* pixels, int count)
{
    for (int i = 0; i < count; ++i)
    {
        unsigned char blue = pixels[i * 4];
        pixels[i * 4] = pixels[i * 4 + 2];
        pixels[i * 4 + 2] = blue;
    }
}
static int nextMipSize(int size)
{
    return size / 2 > 0 ? size / 2 : 1;
}

// 0x004e36f0: 2D and volume textures; the mip filter follows the renderer's texture
// filter setting.
void RenderableTextureGL::load(const char* filename, int skipMipLevels, bool srgb)
{
    AssetProcessor* processor = findAssetProcessor(AssetProcessor::TextureAsset, filename);
    processor->processFile(filename);
    String native = processor->getNativeFile(filename);
    m_filename = filename;
    DDSLoader dds(native.c_str());
    int width = dds.getWidth(), height = dds.getHeight();
    int depth = dds.getDepth();
    int mipLevels = 1;
    if (dds.hasMipMaps())
    {
        mipLevels = dds.getMipMapCount();
        if (mipLevels > 1)
        {
            int required = 1;
            for (int w = width, h = height; w > 1 || h > 1; w /= 2, h /= 2)
                ++required;
            if (mipLevels != required)
                debugPrint("WARNING! %s has invalid number of mipmap levels (%d mipmap levels "
                           "found, %d required)\n",
                           filename, mipLevels, required);
        }
    }
    GLenum format = getTextureFormat(dds.getFormat(), srgb, filename);
    // the texture resolution setting drops the top mip levels of textures that are big
    // enough to have them to spare
    if (dds.getType() == DDSLoader::Texture2D && skipMipLevels > 0)
    {
        if (width < 64 || height < 64 || mipLevels < 3)
            skipMipLevels = 0;
        mipLevels -= skipMipLevels;
        for (int i = 0; i < skipMipLevels; ++i)
        {
            width /= 2;
            height /= 2;
        }
    }
    int textureFilter = Renderer::getActiveRenderer()->m_textureFilter;
    GLint minFilter = GL_LINEAR;
    if (mipLevels >= 2)
        minFilter = textureFilter > 0 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_NEAREST;

    if (dds.getType() == DDSLoader::Texture2D)
    {
        setTexture(new Texture2DGL(width, height, mipLevels, format, minFilter, GL_LINEAR,
                                   GL_REPEAT, GL_RGBA));
        if (textureFilter == Renderer::TextureFilter_Anisotropic && mipLevels > 1 &&
            RenderContextGL::getMaxAnisotropy() > 0.0f)
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                            RenderContextGL::getMaxAnisotropy());
        for (int level = 0; level < mipLevels; ++level)
        {
            int size = dds.getSurfaceSize(level + skipMipLevels);
            unsigned char* data = new unsigned char[size];
            dds.loadSurface(level + skipMipLevels, data);
            if (format == GL_RGBA16F)
            {
                glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA16F, width, height, 0, GL_RGBA,
                             GL_HALF_FLOAT, data);
                checkGLErrors("glTexImage2D");
            }
            else if (isUncompressed(format))
            {
                swapRedAndBlue(data, width * height);
                glTexImage2D(GL_TEXTURE_2D, level, format, width, height, 0, GL_RGBA,
                             GL_UNSIGNED_BYTE, data);
                checkGLErrors("glTexImage2D");
            }
            else
            {
                glCompressedTexImage2D(GL_TEXTURE_2D, level, format, width, height, 0, size, data);
                checkGLErrors("glCompressedTexImage2D");
            }
            delete[] data;
            width = nextMipSize(width);
            height = nextMipSize(height);
        }
    }
    else if (dds.getType() == DDSLoader::Texture3D)
    {
        setTexture(new Texture3DGL(width, height, depth, mipLevels, format, minFilter, GL_LINEAR,
                                   GL_REPEAT, GL_RGBA));
        for (int level = 0; level < mipLevels; ++level)
        {
            int size = dds.getSurfaceSize(level + skipMipLevels);
            unsigned char* data = new unsigned char[size];
            dds.loadSurface(level + skipMipLevels, data);
            if (format == GL_RGBA16F)
            {
                glTexImage3D(GL_TEXTURE_3D, level, GL_RGBA16F, width, height, depth, 0, GL_RGBA,
                             GL_HALF_FLOAT, data);
                checkGLErrors("glTexImage3D");
            }
            else if (isUncompressed(format))
            {
                swapRedAndBlue(data, width * height * depth);
                glTexImage3D(GL_TEXTURE_3D, level, format, width, height, depth, 0, GL_RGBA,
                             GL_UNSIGNED_BYTE, data);
                checkGLErrors("glTexImage3D");
            }
            else
            {
                glCompressedTexImage3D(GL_TEXTURE_3D, level, format, width, height, depth, 0, size,
                                       data);
                checkGLErrors("glCompressedTexImage3D");
            }
            delete[] data;
            width = nextMipSize(width);
            height = nextMipSize(height);
            depth = nextMipSize(depth);
        }
    }
    else
    {
        throw Exception("Unsupported dds texture type (%d) in file %s", dds.getType(), filename);
    }
}
void RenderableTextureGL::reload() {}
void RenderableTextureGL::save(const char* filename) {}
// 0x004e35d0 / 0x004e35e0
int RenderableTextureGL::getWidth() const
{
    return m_pTexture ? m_pTexture->getWidth() : 0;
}
int RenderableTextureGL::getHeight() const
{
    return m_pTexture ? m_pTexture->getHeight() : 0;
}

// ---- RenderableShaderGL ----------------------------------------------------------

// 0x004c4f60
RenderableShaderGL::RenderableShaderGL()
{
    memset(m_programs, 0, sizeof(m_programs));
}
// 0x004c4fe0
RenderableShaderGL::~RenderableShaderGL()
{
    for (int i = 0; i < NumPrograms; ++i)
        delete m_programs[i];
}
void RenderableShaderGL::setProgram(int variant, ShaderProgramGL* program)
{
    if (m_programs[variant] != program)
    {
        delete m_programs[variant];
        m_programs[variant] = program;
    }
}
// 0x004f25c0: the geometry pass in all eight variants, the material/unlit/shadow passes
// with skinning and alpha test, the directional light shadow pass with its own define.
void RenderableShaderGL::initLightPrePassRendererShader(const char* vertexShader,
                                                        const char* geometryShader,
                                                        const char* materialShader,
                                                        const char* unlitShader,
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
    static constexpr const char* defDir[] = {"SHADOW_DIRLIGHT", 0};
    static constexpr const char* defDirAlpha[] = {"SHADOW_DIRLIGHT", "ALPHA_TEST", 0};
    static constexpr const char* defDirSkin[] = {"SHADOW_DIRLIGHT", "SKINNING", 0};
    const char* shadowVertexShader = "shaders/gl/MeshShadow.vsh";
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
    GLuint fsMaterialAlpha =
        RenderContextGL::compileShaderFromFile(materialShader, GL_FRAGMENT_SHADER, defAlpha);
    GLuint fsUnlit = RenderContextGL::compileShaderFromFile(unlitShader, GL_FRAGMENT_SHADER, 0);
    GLuint fsUnlitAlpha =
        RenderContextGL::compileShaderFromFile(unlitShader, GL_FRAGMENT_SHADER, defAlpha);
    GLuint vsShadow =
        RenderContextGL::compileShaderFromFile(shadowVertexShader, GL_VERTEX_SHADER, 0);
    GLuint vsShadowSkin =
        RenderContextGL::compileShaderFromFile(shadowVertexShader, GL_VERTEX_SHADER, defSkin);
    GLuint vsShadowDir =
        RenderContextGL::compileShaderFromFile(shadowVertexShader, GL_VERTEX_SHADER, defDir);
    GLuint vsShadowDirSkin =
        RenderContextGL::compileShaderFromFile(shadowVertexShader, GL_VERTEX_SHADER, defDirSkin);
    GLuint fsShadow = RenderContextGL::compileShaderFromFile(shadowShader, GL_FRAGMENT_SHADER, 0);
    GLuint fsShadowAlpha =
        RenderContextGL::compileShaderFromFile(shadowShader, GL_FRAGMENT_SHADER, defAlpha);
    GLuint fsShadowDir =
        RenderContextGL::compileShaderFromFile(shadowShader, GL_FRAGMENT_SHADER, defDir);
    GLuint fsShadowDirAlpha =
        RenderContextGL::compileShaderFromFile(shadowShader, GL_FRAGMENT_SHADER, defDirAlpha);

    setProgram(0, new ShaderProgramGL(vs, fsGeom));
    setProgram(Skinning, new ShaderProgramGL(vsSkin, fsGeom));
    setProgram(NormalMap, new ShaderProgramGL(vsNormal, fsGeomNormal));
    setProgram(NormalMap | Skinning, new ShaderProgramGL(vsNormalSkin, fsGeomNormal));
    setProgram(AlphaTest, new ShaderProgramGL(vs, fsGeomAlpha));
    setProgram(AlphaTest | Skinning, new ShaderProgramGL(vsSkin, fsGeomAlpha));
    setProgram(NormalMap | AlphaTest, new ShaderProgramGL(vsNormal, fsGeomNormalAlpha));
    setProgram(NormalMap | AlphaTest | Skinning,
               new ShaderProgramGL(vsNormalSkin, fsGeomNormalAlpha));
    setProgram(MaterialPass, new ShaderProgramGL(vs, fsMaterial));
    setProgram(MaterialPass | Skinning, new ShaderProgramGL(vsSkin, fsMaterial));
    setProgram(MaterialPass | AlphaTest, new ShaderProgramGL(vs, fsMaterialAlpha));
    setProgram(MaterialPass | AlphaTest | Skinning, new ShaderProgramGL(vsSkin, fsMaterialAlpha));
    setProgram(UnlitPass, new ShaderProgramGL(vs, fsUnlit));
    setProgram(UnlitPass | Skinning, new ShaderProgramGL(vsSkin, fsUnlit));
    setProgram(UnlitPass | AlphaTest, new ShaderProgramGL(vs, fsUnlitAlpha));
    setProgram(UnlitPass | AlphaTest | Skinning, new ShaderProgramGL(vsSkin, fsUnlitAlpha));
    setProgram(ShadowPass, new ShaderProgramGL(vsShadow, fsShadow));
    setProgram(ShadowPass | Skinning, new ShaderProgramGL(vsShadowSkin, fsShadow));
    setProgram(ShadowPass | AlphaTest, new ShaderProgramGL(vsShadow, fsShadowAlpha));
    setProgram(ShadowPass | AlphaTest | Skinning, new ShaderProgramGL(vsShadowSkin, fsShadowAlpha));
    setProgram(ShadowDirLight, new ShaderProgramGL(vsShadowDir, fsShadowDir));
    setProgram(ShadowDirLight | Skinning, new ShaderProgramGL(vsShadowDirSkin, fsShadowDir));
    setProgram(ShadowDirLight | AlphaTest, new ShaderProgramGL(vsShadowDir, fsShadowDirAlpha));
    setProgram(ShadowDirLight | AlphaTest | Skinning,
               new ShaderProgramGL(vsShadowDirSkin, fsShadowDirAlpha));
    GLuint shaders[] = {vs,         vsSkin,          vsNormal,    vsNormalSkin,
                        fsGeom,     fsGeomNormal,    fsGeomAlpha, fsGeomNormalAlpha,
                        fsMaterial, fsMaterialAlpha, fsUnlit,     fsUnlitAlpha,
                        vsShadow,   vsShadowSkin,    vsShadowDir, vsShadowDirSkin,
                        fsShadow,   fsShadowAlpha,   fsShadowDir, fsShadowDirAlpha};
    for (size_t i = 0; i < sizeof(shaders) / sizeof(shaders[0]); ++i)
        glDeleteShader(shaders[i]);
}
// 0x004f3110
void RenderableShaderGL::initNotebookRendererShader(const char* vertexShader,
                                                    const char* fragmentShader,
                                                    const char* unlitShader)
{
    if (!vertexShader)
        vertexShader = "shaders/gl/NotebookRenderer.vsh";
    if (!fragmentShader)
        fragmentShader = "shaders/gl/NotebookRenderer.fsh";
    if (!unlitShader)
        unlitShader = "shaders/gl/NotebookRendererUnlit.fsh";
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
    GLuint fs = RenderContextGL::compileShaderFromFile(fragmentShader, GL_FRAGMENT_SHADER, 0);
    GLuint fsNormal =
        RenderContextGL::compileShaderFromFile(fragmentShader, GL_FRAGMENT_SHADER, defNormal);
    GLuint fsAlpha =
        RenderContextGL::compileShaderFromFile(fragmentShader, GL_FRAGMENT_SHADER, defAlpha);
    GLuint fsNormalAlpha =
        RenderContextGL::compileShaderFromFile(fragmentShader, GL_FRAGMENT_SHADER, defNormalAlpha);
    GLuint fsUnlit = RenderContextGL::compileShaderFromFile(unlitShader, GL_FRAGMENT_SHADER, 0);
    GLuint fsUnlitAlpha =
        RenderContextGL::compileShaderFromFile(unlitShader, GL_FRAGMENT_SHADER, defAlpha);
    setProgram(NotebookPass, new ShaderProgramGL(vs, fs));
    setProgram(NotebookPass | Skinning, new ShaderProgramGL(vsSkin, fs));
    setProgram(NotebookPass | NormalMap, new ShaderProgramGL(vsNormal, fsNormal));
    setProgram(NotebookPass | NormalMap | Skinning, new ShaderProgramGL(vsNormalSkin, fsNormal));
    setProgram(NotebookPass | AlphaTest, new ShaderProgramGL(vs, fsAlpha));
    setProgram(NotebookPass | AlphaTest | Skinning, new ShaderProgramGL(vsSkin, fsAlpha));
    setProgram(NotebookPass | NormalMap | AlphaTest, new ShaderProgramGL(vsNormal, fsNormalAlpha));
    setProgram(NotebookPass | NormalMap | AlphaTest | Skinning,
               new ShaderProgramGL(vsNormalSkin, fsNormalAlpha));
    setProgram(NotebookPass | UnlitPass, new ShaderProgramGL(vs, fsUnlit));
    setProgram(NotebookPass | UnlitPass | Skinning, new ShaderProgramGL(vsSkin, fsUnlit));
    setProgram(NotebookPass | UnlitPass | AlphaTest, new ShaderProgramGL(vs, fsUnlitAlpha));
    setProgram(NotebookPass | UnlitPass | AlphaTest | Skinning,
               new ShaderProgramGL(vsSkin, fsUnlitAlpha));
    GLuint shaders[] = {vs,       vsSkin,  vsNormal,      vsNormalSkin, fs,
                        fsNormal, fsAlpha, fsNormalAlpha, fsUnlit,      fsUnlitAlpha};
    for (size_t i = 0; i < sizeof(shaders) / sizeof(shaders[0]); ++i)
        glDeleteShader(shaders[i]);
}
// 0x004f3710
void RenderableShaderGL::initPostProcessShader(const char* fragmentShader)
{
    setProgram(0, new ShaderProgramGL("shaders/gl/PostProcess.vsh", fragmentShader));
}

// ---- RenderableMeshGL ------------------------------------------------------------

// 0x004e32b0
RenderableMeshGL::RenderableMeshGL()
    : m_vertexBuffer(0), m_indexBuffer(0), m_vertexArray(0), m_numIndices(0), m_indexSize(2),
      m_numVertices(0), m_stride(0), m_normalOffset(-1), m_tangentOffset(-1), m_texcoordOffset_(-1),
      m_colorOffset(-1), m_boneIndicesOffset(-1), m_boneWeightsOffset(-1), m_skinned(false),
      m_texcoordScale(1, 1), m_texcoordOffset(0, 0), m_debugVertexBuffer(0), m_debugIndexBuffer(0),
      m_debugVertexArray(0), m_debugLineCount(0), m_debugLineKind(0)
{
}
// 0x004e3380
RenderableMeshGL::~RenderableMeshGL()
{
    glDeleteBuffers(1, &m_vertexBuffer);
    glDeleteBuffers(1, &m_indexBuffer);
    glDeleteVertexArrays(1, &m_vertexArray);
    glDeleteBuffers(1, &m_debugVertexBuffer);
    glDeleteBuffers(1, &m_debugIndexBuffer);
    glDeleteVertexArrays(1, &m_debugVertexArray);
}

static inline unsigned char packUnitByte(float v)
{
    int i = (int)((v + 1.0f) * 127.5f);
    return (unsigned char)(i < 0 ? 0 : (i > 255 ? 255 : i));
}
static inline short packShort(float v)
{
    int i = (int)v;
    return (short)(i < -0x8000 ? -0x8000 : (i > 0x7fff ? 0x7fff : i));
}

// The range of the texture coordinates, which are stored as 16 bit integers across it: a
// coordinate is offset + short * scale. A flat range keeps the scale at 1.
static void getTexcoordQuantization(const Vec2* uv, int count, Vec2& scale, Vec2& offset)
{
    Vec2 mn(100000.0f, 100000.0f), mx(-100000.0f, -100000.0f);
    for (int i = 0; i < count; ++i)
    {
        if (uv[i].x <= mn.x)
            mn.x = uv[i].x;
        if (uv[i].y <= mn.y)
            mn.y = uv[i].y;
        if (uv[i].x >= mx.x)
            mx.x = uv[i].x;
        if (uv[i].y >= mx.y)
            mx.y = uv[i].y;
    }
    scale.set((mx.x - mn.x) / 65535.0f, (mx.y - mn.y) / 65535.0f);
    offset.set(scale.x * 32768.0f + mn.x, scale.y * 32768.0f + mn.y);
    if (scale.x == 0.0f)
        scale.x = 1.0f;
    if (scale.y == 0.0f)
        scale.y = 1.0f;
}

// Four bone indices and weights as bytes. The weights stay normalised after the
// quantisation: the last non-zero one absorbs the rounding error.
static void packBones(const int* indices, const float* weights, int components,
                      unsigned char* outIndices, unsigned char* outWeights)
{
    int sum = 0;
    for (int c = 0; c < 4; ++c)
    {
        outIndices[c] = c < components ? (unsigned char)indices[c] : 0;
        int w = c < components ? (int)(weights[c] * 255.0f) : 0;
        outWeights[c] = (unsigned char)w;
        sum += outWeights[c];
    }
    if (sum != 255)
    {
        int last = outWeights[1] != 0 ? 1 : 0;
        if (outWeights[2] != 0)
            last = 2;
        if (outWeights[3] != 0)
            last = 3;
        outWeights[last] = (unsigned char)(outWeights[last] + (255 - sum));
    }
}

// One attribute of the interleaved vertex; offset -1 = the mesh does not have it.
static void enableAttribute(ShaderProgramGL::Attribute location, int size, GLenum type, int stride,
                            int offset)
{
    if (offset < 0)
        return;
    glEnableVertexAttribArray(location);
    glVertexAttribPointer(location, size, type, GL_FALSE, stride, (const void*)(intptr_t)offset);
}

// 0x004e2790
void RenderableMeshGL::init(Mesh& mesh, bool keepSourceData)
{
    if (m_vertexBuffer)
        glDeleteBuffers(1, &m_vertexBuffer);
    if (m_indexBuffer)
        glDeleteBuffers(1, &m_indexBuffer);
    if (m_vertexArray)
        glDeleteVertexArrays(1, &m_vertexArray);
    const Vec3* positions = (const Vec3*)mesh.getVertexArray(Mesh::Position, Mesh::TypeFloat, 3);
    const Vec3* normals = (const Vec3*)mesh.getVertexArray(Mesh::Normal, Mesh::TypeFloat, 3);
    const Vec3* tangents = (const Vec3*)mesh.getVertexArray(Mesh::Tangent, Mesh::TypeFloat, 3);
    const Vec3* bitangents = (const Vec3*)mesh.getVertexArray(Mesh::Bitangent, Mesh::TypeFloat, 3);
    const Vec2* texcoords = (const Vec2*)mesh.getVertexArray(Mesh::Texcoord0, Mesh::TypeFloat, 2);
    const unsigned char* colors =
        (const unsigned char*)mesh.getVertexArray(Mesh::Color, Mesh::TypeByte, 4);
    const int* boneIndices = (const int*)mesh.getVertexArray(Mesh::BoneIndices, Mesh::TypeInt, -1);
    const float* boneWeights =
        (const float*)mesh.getVertexArray(Mesh::BoneWeights, Mesh::TypeFloat, -1);
    int boneComponents = mesh.getVertexArrayInfo(Mesh::BoneIndices).components;
    m_skinned = boneIndices && boneWeights && boneComponents > 0;
    m_numVertices = mesh.getNumVertices();

    // the interleaved layout: a float position, then 4 bytes per attribute the mesh has
    // (the texture coordinates as two shorts)
    m_normalOffset = m_tangentOffset = m_texcoordOffset_ = m_colorOffset = -1;
    m_boneIndicesOffset = m_boneWeightsOffset = -1;
    int stride = 12;
    if (normals)
    {
        m_normalOffset = stride;
        stride += 4;
    }
    if (tangents)
    {
        m_tangentOffset = stride;
        stride += 4;
    }
    if (texcoords)
    {
        m_texcoordOffset_ = stride;
        stride += 4;
    }
    if (colors)
    {
        m_colorOffset = stride;
        stride += 4;
    }
    if (m_skinned)
    {
        m_boneIndicesOffset = stride;
        m_boneWeightsOffset = stride + 4;
        stride += 8;
    }
    m_stride = stride;
    m_texcoordScale.set(1.0f, 1.0f);
    m_texcoordOffset.set(0.0f, 0.0f);
    if (texcoords)
        getTexcoordQuantization(texcoords, m_numVertices, m_texcoordScale, m_texcoordOffset);

    glGenBuffers(1, &m_vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)m_numVertices * stride, 0, GL_STATIC_DRAW);
    char* vertices = (char*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    for (int i = 0; i < m_numVertices; ++i)
    {
        char* vertex = vertices + i * stride;
        memcpy(vertex, &positions[i], 12);
        if (normals)
        {
            unsigned char* n = (unsigned char*)(vertex + m_normalOffset);
            n[0] = packUnitByte(normals[i].x);
            n[1] = packUnitByte(normals[i].y);
            n[2] = packUnitByte(normals[i].z);
            n[3] = 0;
        }
        if (tangents && bitangents)
        {
            // the handedness of the tangent frame goes into w
            float handedness =
                dot(cross(normals[i], tangents[i]), bitangents[i]) < 0.0f ? -1.0f : 1.0f;
            unsigned char* t = (unsigned char*)(vertex + m_tangentOffset);
            t[0] = packUnitByte(tangents[i].x);
            t[1] = packUnitByte(tangents[i].y);
            t[2] = packUnitByte(tangents[i].z);
            t[3] = packUnitByte(handedness);
        }
        if (texcoords)
        {
            short* t = (short*)(vertex + m_texcoordOffset_);
            t[0] = packShort((texcoords[i].x - m_texcoordOffset.x) / m_texcoordScale.x);
            t[1] = packShort((texcoords[i].y - m_texcoordOffset.y) / m_texcoordScale.y);
        }
        if (colors)
            memcpy(vertex + m_colorOffset, colors + i * 4, 4);
        if (m_skinned)
            packBones(boneIndices + i * boneComponents, boneWeights + i * boneComponents,
                      boneComponents, (unsigned char*)(vertex + m_boneIndicesOffset),
                      (unsigned char*)(vertex + m_boneWeightsOffset));
    }
    glUnmapBuffer(GL_ARRAY_BUFFER);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // 16 bit indices when the vertices allow it
    const Array<int>& indices = mesh.getIndices();
    m_numIndices = indices.size();
    m_indexSize = m_numVertices > 0xffff ? 4 : 2;
    glGenBuffers(1, &m_indexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)m_indexSize * m_numIndices, 0,
                 GL_STATIC_DRAW);
    void* indexData = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
    if (m_indexSize == 2)
    {
        unsigned short* shortIndices = (unsigned short*)indexData;
        for (int i = 0; i < m_numIndices; ++i)
            shortIndices[i] = (unsigned short)indices[i];
    }
    else
    {
        memcpy(indexData, indices.data(), m_numIndices * 4);
    }
    glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    m_segments.clear();
    for (int i = 0; i < mesh.getNumSegments(); ++i)
    {
        const MeshSegment& source = mesh.getSegment(i);
        Segment segment;
        segment.material = source.material;
        segment.mode = source.primitiveType == Mesh::TriangleList
                           ? GL_TRIANGLES
                           : (source.primitiveType == Mesh::LineList ? GL_LINES : GL_POINTS);
        segment.firstIndex = source.firstIndex;
        segment.primitiveCount = source.numTriangles;
        m_segments.push_back(segment);
    }
    m_bounds = mesh.getBoundingBox();
    m_sourceData.reset(keepSourceData ? new Mesh(mesh) : 0);

    glGenVertexArrays(1, &m_vertexArray);
    glBindVertexArray(m_vertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
    enableAttribute(ShaderProgramGL::A_position, 3, GL_FLOAT, m_stride, 0);
    enableAttribute(ShaderProgramGL::A_normal, 4, GL_UNSIGNED_BYTE, m_stride, m_normalOffset);
    enableAttribute(ShaderProgramGL::A_tangent, 4, GL_UNSIGNED_BYTE, m_stride, m_tangentOffset);
    enableAttribute(ShaderProgramGL::A_texcoord, 2, GL_SHORT, m_stride, m_texcoordOffset_);
    enableAttribute(ShaderProgramGL::A_color, 4, GL_UNSIGNED_BYTE, m_stride, m_colorOffset);
    enableAttribute(ShaderProgramGL::A_boneIndices, 4, GL_UNSIGNED_BYTE, m_stride,
                    m_boneIndicesOffset);
    enableAttribute(ShaderProgramGL::A_boneWeights, 4, GL_UNSIGNED_BYTE, m_stride,
                    m_boneWeightsOffset);
    glBindVertexArray(0);
}
// 0x004e3370 (bounds) and the base accessors
Mesh* RenderableMeshGL::getSourceData()
{
    return m_sourceData.get();
}
const AABox3& RenderableMeshGL::getBoundingBox() const
{
    return m_bounds;
}
// 0x004e31f0
Array<SharedPtr<Material>> RenderableMeshGL::getMaterials() const
{
    Array<SharedPtr<Material>> materials;
    materials.reserve(m_segments.size());
    for (int i = 0; i < m_segments.size(); ++i)
        materials.push_back(m_segments[i].material);
    return materials;
}
void RenderableMeshGL::drawNormals(const Matrix4x3& m) {}
void RenderableMeshGL::drawTangents(const Matrix4x3& m) {}

// ---- CommonResourcesGL -----------------------------------------------------------

RenderableTextureGL* CommonResourcesGL::BlackMap = 0;
RenderableTextureGL* CommonResourcesGL::GrayMap = 0;
RenderableTextureGL* CommonResourcesGL::WhiteMap = 0;
RenderableTextureGL* CommonResourcesGL::DefaultNormalMap = 0;
RenderableTextureGL* CommonResourcesGL::RotMap = 0;
RenderableTextureGL* CommonResourcesGL::NoiseMap = 0;
RenderableMeshGL* CommonResourcesGL::SphereMesh = 0;
RenderableMeshGL* CommonResourcesGL::ConeMesh = 0;
Material* CommonResourcesGL::DefaultMaterial = 0;
Font* CommonResourcesGL::DefaultFont = 0;

constexpr int RotTexSize = 32;

// 0x004e4460
CommonResourcesGL::CommonResourcesGL()
{
    // one pixel textures: black, dark grey, white and the flat normal (green/alpha encoded)
    static const Color pixels[4] = {Color(0, 0, 0, 255), Color(64, 64, 64, 255),
                                    Color(255, 255, 255, 255), Color(0, 128, 0, 128)};
    RenderableTextureGL** targets[4] = {&BlackMap, &GrayMap, &WhiteMap, &DefaultNormalMap};
    for (int i = 0; i < 4; ++i)
    {
        Image image(1, 1);
        image.setPixel(0, 0, pixels[i]);
        SharedPtr<RenderableTextureGL> t(new RenderableTextureGL);
        t->init(image);
        m_textures.push_back(t);
        *targets[i] = t.get();
    }
    {
        // RotTexSize^2 rotation vectors (cos, -sin, sin, cos) for the ambient occlusion kernel
        Image rot(RotTexSize, RotTexSize);
        for (int i = 0; i < RotTexSize * RotTexSize; ++i)
        {
            float angle = (float)g_rotTex[i] * PI * 2.0f / 255.0f;
            float cosA = std::cos(angle), sinA = std::sin(angle);
            unsigned char cosByte = (unsigned char)(int)((cosA * 0.5f + 0.5f) * 255.0f);
            rot.setPixel(i % RotTexSize, i / RotTexSize,
                         Color(cosByte, (unsigned char)(int)((-sinA * 0.5f + 0.5f) * 255.0f),
                               (unsigned char)(int)((sinA * 0.5f + 0.5f) * 255.0f), cosByte));
        }
        SharedPtr<RenderableTextureGL> t(new RenderableTextureGL);
        t->init(rot);
        m_textures.push_back(t);
        RotMap = t.get();
    }
    {
        // grey noise from the default seeded generator
        Image noise(RotTexSize, RotTexSize);
        MersenneTwister random;
        for (int i = 0; i < RotTexSize * RotTexSize; ++i)
        {
            unsigned char v = (unsigned char)(random.genrand_int32() >> 1);
            noise.setPixel(i % RotTexSize, i / RotTexSize, Color(v, v, v, 255));
        }
        SharedPtr<RenderableTextureGL> t(new RenderableTextureGL);
        t->init(noise);
        m_textures.push_back(t);
        NoiseMap = t.get();
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
        SharedPtr<Material> material(Material::getMaterialByName("default"));
        m_materials.push_back(material);
        DefaultMaterial = material.get();
    }
    {
        SharedPtr<Font> font(new Font(Font::Consolas12pt));
        m_fonts.push_back(font);
        DefaultFont = font.get();
    }
}
// 0x004e5380
CommonResourcesGL::~CommonResourcesGL()
{
    BlackMap = 0;
    GrayMap = 0;
    WhiteMap = 0;
    DefaultNormalMap = 0;
    RotMap = 0;
    NoiseMap = 0;
    SphereMesh = 0;
    ConeMesh = 0;
    DefaultMaterial = 0;
    DefaultFont = 0;
}

// ---- RendererGL ------------------------------------------------------------------

// 0x004c5ab0
RendererGL::RendererGL()
    : m_pContext(0), m_pGraphics(0), m_pCommonResources(0), m_pRenderVisitor(0),
      m_pImmediateMode(0), m_pOcclusionCulling(0), m_pNotebookRenderer(0),
      m_pLightPrePassRenderer(0), m_pRefractionBuffer(0)
{
}
// 0x004c5b70: the reverse of init
RendererGL::~RendererGL()
{
    m_blitProgram.reset(0);
    m_waterRefraction.reset(0);
    m_tonemapper.reset(0);
    m_pAmbientOcclusion.reset(0);
    m_fogFilter.reset(0);
    delete m_pLightPrePassRenderer;
    delete m_pNotebookRenderer;
    delete m_pOcclusionCulling;
    delete m_pImmediateMode;
    delete m_pRenderVisitor;
    delete m_pCommonResources;
    if (Graphics::sm_pActive == m_pGraphics)
        Graphics::sm_pActive = 0;
    delete m_pGraphics;
    delete m_pContext;
}
// 0x004c5e10
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
    m_pOcclusionCulling = new OcclusionCullingGL;
    m_fogFilter.reset(new FogFilterGL(m_pContext));
    m_blitProgram.reset(
        new ShaderProgramGL("shaders/gl/BlitBuffer.vsh", "shaders/gl/BlitBuffer.fsh"));
    // the notebook renderer always exists: Renderer.setFlag(force_notebook_mode) can
    // switch to it at any time
    m_pNotebookRenderer = new NotebookRendererGL(m_pContext);
    m_pNotebookRenderer->resizeRenderBuffers(config.width, config.height);
    if (!config.notebookMode)
    {
        m_pLightPrePassRenderer =
            new LightPrePassRendererGL(m_pContext, config.width, config.height);
        m_pAmbientOcclusion.reset(
            new ScreenSpaceAmbientOcclusionGL(m_pContext, config.width, config.height));
        m_tonemapper.reset(new TonemapperGL(m_pContext));
        m_waterRefraction.reset(new WaterRefractionGL(m_pContext));
    }
    // the original compresses .tga through squish (TextureAssetProcessorSquish); the
    // shipped assets are .dds already
    registerAssetProcessor(AssetProcessor::TextureAsset, "tga", "dds");
    registerAssetProcessor(AssetProcessor::TextureAsset, "dds", "dds");
    Graphics::sm_pActive = m_pGraphics;
    // Debugging aid: GRIMROCK_DEBUG_BUFFER=normal|glossiness|light|ssao sets the flag that
    // makes renderScene draw that intermediate buffer instead of the frame, which is what
    // Renderer.setFlag("draw_<name>_buffer") does from Lua.
    if (const char* debugBuffer = getenv("GRIMROCK_DEBUG_BUFFER"))
    {
        static const struct
        {
            const char* name;
            int flag;
        } buffers[] = {{"normal", Flag_DrawNormalBuffer},
                       {"glossiness", Flag_DrawGlossinessBuffer},
                       {"light", Flag_DrawLightBuffer},
                       {"ssao", Flag_DrawAmbientOcclusionBuffer}};
        for (size_t i = 0; i < sizeof(buffers) / sizeof(buffers[0]); ++i)
            if (strcmp(debugBuffer, buffers[i].name) == 0)
                m_flags |= buffers[i].flag;
    }
    glFrontFace(GL_CW);
    checkGLErrors("glFrontFace");
}
// 0x004c4e60
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
// 0x004c4590
void RendererGL::resizeRenderBuffers(int width, int height)
{
    m_config.width = width;
    m_config.height = height;
    m_pContext->resizeWindow(width, height);
    if (m_pLightPrePassRenderer)
    {
        m_pLightPrePassRenderer->resizeRenderBuffers(width, height);
        m_pAmbientOcclusion->resizeRenderBuffers(width, height);
    }
    if (m_pNotebookRenderer)
        m_pNotebookRenderer->resizeRenderBuffers(width, height);
}
bool RendererGL::isReadyToRender()
{
    return true;
}
// 0x004c4530
void RendererGL::setRefractionBuffer(RenderableTexture* buffer)
{
    m_pRefractionBuffer = buffer ? ((RenderableTextureGL*)buffer)->getTexture() : 0;
}
// 0x004c4550
void RendererGL::setRefractionClipPlane(const Vec4& plane)
{
    if (m_waterRefraction)
        m_waterRefraction->setClippingPlane(plane);
}
// 0x004c47e0
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
    memset(&g_renderStats, 0, sizeof(g_renderStats));
    im::prepare(m_config.width, m_config.height);
}
// 0x004c5050
void RendererGL::renderScene(Scene& scene, Camera& camera, RenderableTexture* target, int passMask)
{
    m_pContext->setScene(&scene, &camera);
    m_pContext->nextFrame();
    RenderVisitor& visitor = *m_pRenderVisitor;
    visitor.gatherEntities(scene, camera, (unsigned int)passMask);
    if (m_occlusionCulling)
    {
        m_pOcclusionCulling->renderOccluders(visitor.m_occluders.data(), visitor.m_occluders.size(),
                                             camera);
        visitor.m_meshes.resize(m_pOcclusionCulling->testAABBs(
            (RenderEntity**)visitor.m_meshes.data(), visitor.m_meshes.size(), camera));
        g_renderStats.visibleMeshes = visitor.m_meshes.size();
        visitor.m_lights.resize(m_pOcclusionCulling->testAABBs(
            (RenderEntity**)visitor.m_lights.data(), visitor.m_lights.size(), camera));
        g_renderStats.visibleLights = visitor.m_lights.size();
        visitor.m_particles.resize(m_pOcclusionCulling->testAABBs(
            (RenderEntity**)visitor.m_particles.data(), visitor.m_particles.size(), camera));
        g_renderStats.visibleParticleSystems = visitor.m_particles.size();
    }
    for (int i = 0; i < visitor.m_lights.size(); ++i)
    {
        LightEntity* light = visitor.m_lights[i];
        if (light->getLightType() != LightEntity::Point)
            continue;
        unsigned int faces = light->getVisibleFaceMask(camera);
        if (m_occlusionCulling)
            faces &= m_pOcclusionCulling->testPointLight(*light, camera);
        light->setVisibleFaces(faces & 0xff);
    }
    Texture2DGL* targetTexture =
        target ? (Texture2DGL*)((RenderableTextureGL*)target)->getTexture() : 0;

    if (isNotebookMode())
    {
        // forward rendering straight into the target inside the viewport
        int viewportY;
        if (!targetTexture)
        {
            m_pContext->setRenderTarget(RenderContextGL::DefaultFrameBuffer);
            viewportY = m_config.height - m_viewportHeight - m_viewportY;
        }
        else
        {
            GLuint depth = m_pNotebookRenderer->getDepthBuffer();
            m_pContext->setRenderTarget(targetTexture, 0, depth, depth);
            viewportY = targetTexture->getHeight() - m_viewportHeight - m_viewportY;
        }
        glViewport(m_viewportX, viewportY, m_viewportWidth, m_viewportHeight);
        checkGLErrors("glViewport");
        glScissor(m_viewportX, viewportY, m_viewportWidth, m_viewportHeight);
        checkGLErrors("glScissor");
        glEnable(GL_SCISSOR_TEST);
        glClearColor(m_clearColor.r / 255.0f, m_clearColor.g / 255.0f, m_clearColor.b / 255.0f,
                     m_clearColor.a / 255.0f);
        glClearDepth(1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);
        NotebookRendererGL* nb = m_pNotebookRenderer;
        nb->m_diffuseMapping = m_diffuseMapping;
        nb->m_normalMapping = m_normalMapping;
        nb->m_width = m_config.width;
        nb->m_height = m_config.height;
        if (camera.getInverseCulling())
            glCullFace(GL_FRONT);
        nb->m_fogColor = m_fogFilter->m_fogColor;
        nb->m_fogStart = m_fogFilter->m_fogRange.x;
        nb->m_fogEnd = m_fogFilter->m_fogRange.y;
        nb->renderOpaquePass(camera, visitor);
        nb->renderTransparentPass(camera, visitor, NotebookRendererGL::TransparentPass2);
        // the last pass is drawn without fog
        nb->m_fogColor.set(0, 0, 0);
        nb->renderTransparentPass(camera, visitor, NotebookRendererGL::TransparentPass1);
        if (camera.getInverseCulling())
            glCullFace(GL_BACK);
        im::flushDebugDraw(camera.getViewProjectionMatrix(), CommonResourcesGL::DefaultFont);
        glViewport(0, 0, m_config.width, m_config.height);
        checkGLErrors("glViewport");
        glScissor(0, 0, m_config.width, m_config.height);
        checkGLErrors("glScissor");
        checkGLErrors("renderScene");
        return;
    }

    LightPrePassRendererGL* lpp = m_pLightPrePassRenderer;
    lpp->setViewport(m_viewportX, m_viewportY, m_viewportWidth, m_viewportHeight);
    lpp->m_clearColor = m_clearColor;
    lpp->m_diffuseMapping = m_diffuseMapping;
    lpp->m_normalMapping = m_normalMapping;
    lpp->m_textureFilter = m_textureFilter;
    lpp->m_renderMeshes = m_renderMeshes;
    lpp->m_renderShadows = m_renderShadows && !g_debugNoShadows;
    lpp->m_shadowQuality = m_shadowQuality;
    if (camera.getInverseCulling())
        glCullFace(GL_FRONT);
    Texture2DGL* geometryBuffer = (Texture2DGL*)lpp->getGeometryBuffer()->getTexture();
    lpp->renderGeometryPass(camera, visitor);
    lpp->renderLightPass(camera, visitor);
    lpp->renderMaterialPass(camera, visitor, LightPrePassRendererGL::MaterialPass_AmbientOcclusion);
    // the linear depth is also the fog filter's input
    m_pAmbientOcclusion->prepareDepth(geometryBuffer);
    if (m_ambientOcclusion)
        m_pAmbientOcclusion->render(camera, geometryBuffer, lpp->getFrameBuffer());
    lpp->renderTransparentPass(camera, visitor, LightPrePassRendererGL::TransparentPass_First);
    lpp->renderMaterialPass(camera, visitor,
                            LightPrePassRendererGL::MaterialPass_NoAmbientOcclusion);
    if (m_pRefractionBuffer)
        m_waterRefraction->render(geometryBuffer, lpp->getFrameBuffer(),
                                  (Texture2DGL*)m_pRefractionBuffer);
    lpp->renderTransparentPass(camera, visitor, LightPrePassRendererGL::TransparentPass_Second);
    if (m_fog)
        m_fogFilter->render(m_pAmbientOcclusion->getDepthBuffer(), lpp->getFrameBuffer());
    lpp->renderTransparentPass(camera, visitor, LightPrePassRendererGL::TransparentPass_Last);
    if (camera.getInverseCulling())
        glCullFace(GL_BACK);
    im::flushDebugDraw(camera.getViewProjectionMatrix(), CommonResourcesGL::DefaultFont);
    m_tonemapper->render(lpp->getFrameBuffer(), targetTexture);
    glViewport(0, 0, m_config.width, m_config.height);
    checkGLErrors("glViewport");
    glScissor(0, 0, m_config.width, m_config.height);
    checkGLErrors("glScissor");
    if (m_flags & Flag_DrawNormalBuffer)
        drawBuffer(geometryBuffer, 1.0f);
    else if (m_flags & Flag_DrawGlossinessBuffer)
        drawBuffer(lpp->getGlossinessBuffer(), 0.01f);
    else if (m_flags & Flag_DrawLightBuffer)
        drawBuffer(lpp->getLightBuffer(), 1.0f);
    else if (m_flags & Flag_DrawAmbientOcclusionBuffer)
        drawBuffer(m_pAmbientOcclusion->getResultBuffer(), 1.0f);
    checkGLErrors("renderScene");
    m_pContext->setScene(0, 0);
}
// 0x004c48d0
RenderableTexture* RendererGL::renderStaticShadowMap(LightEntity& light, int size, float bias,
                                                     int flags, float* extents)
{
    if (!m_pLightPrePassRenderer)
        return 0;
    TextureGL* map = m_pLightPrePassRenderer->renderStaticShadowMap(light, size, extents);
    RenderableTextureGL* texture = new RenderableTextureGL;
    texture->setTexture(map);
    return texture;
}
// 0x004c4980
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
void RendererGL::saveScreenShot(const char* filename) {}
RenderableMesh* RendererGL::createRenderableMesh()
{
    return new RenderableMeshGL;
}
RenderableTexture* RendererGL::createRenderableTexture()
{
    return new RenderableTextureGL;
}
RenderableShader* RendererGL::createRenderableShader()
{
    return new RenderableShaderGL;
}
RenderWindow* RendererGL::createRenderWindow(const Window& window)
{
    return new RenderWindowGL;
}
// 0x004c4620: rgba_8 or rgba_16f target with nearest filtering.
RenderableTexture* RendererGL::createRenderBuffer(int width, int height, int format)
{
    static constexpr GLint formats[2] = {GL_RGBA8, GL_RGBA16F};
    RenderableTextureGL* texture = new RenderableTextureGL;
    texture->setTexture(new Texture2DGL(width, height, 1, formats[format & 1], GL_NEAREST,
                                        GL_NEAREST, GL_CLAMP_TO_EDGE, GL_RGBA));
    return texture;
}
// 0x004c4cd0
VPXPlayer* RendererGL::createVPXPlayer()
{
    return new VPXPlayerGL(m_pContext);
}
SSAOFilter* RendererGL::getSSAOFilter()
{
    return m_pAmbientOcclusion.get();
}
FogFilter* RendererGL::getFogFilter()
{
    return m_fogFilter.get();
}
Tonemapper* RendererGL::getTonemapper()
{
    return m_tonemapper.get();
}
// 0x004c4530
RenderableTexture* RendererGL::getGeometryBuffer()
{
    return m_pLightPrePassRenderer ? m_pLightPrePassRenderer->getGeometryBuffer() : 0;
}
// 0x004c4fd0
void RendererGL::enumerateResolutions(Array<std::pair<int, int>>& resolutions)
{
    RenderContextSDL::enumerateResolutions(resolutions);
    std::sort(resolutions.begin(), resolutions.end());
}
// 0x004c4520: a constant 256 MB; the game picks its texture resolution from it.
int RendererGL::getAvailableTextureMemory()
{
    return ReportedTextureMemory;
}
// 0x004c4990
void RendererGL::drawBuffer(TextureGL* texture, float scale)
{
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    m_pContext->setBlendMode(RenderContextGL::Blend_Opaque);
    m_pContext->useProgram(m_blitProgram.get());
    m_blitProgram->setUniform("g_color", Vec4(scale, scale, scale, 1.0f));
    m_pContext->setUniformTexture(ShaderProgramGL::U_diffuseMap, texture, -1, -1, 0);
    m_pContext->drawRect();
}

} // namespace engine
