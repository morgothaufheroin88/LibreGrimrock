// Reconstructed from grimrock2.exe VPXPlayerGL.cpp.
#include "engine/VPXPlayerGL.h"
#include "core/Exception.h"
#include "core/FileSystem.h"
#include "core/Sys.h"
#include "engine/Renderer.h"
#include <cmath>
#include <cstring>

#if GRIMROCK_HAVE_VPX
#include <vpx/vp8dx.h>
#endif

namespace engine
{

using namespace core;

constexpr int IVFHeaderSize = 32;
constexpr int IVFFrameHeaderSize = 12;
constexpr float DefaultFrameRate = 30.0f;

// 0x004f20f0
VPXPlayerGL::VPXPlayerGL(RenderContextGL* context)
    : m_pContext(context), m_pProgram(0), m_pFile(0), m_pTextureY(0), m_pTextureU(0),
      m_pTextureV(0), m_width(0), m_height(0), m_frameRate(DefaultFrameRate), m_frame(0),
      m_pBufferY(0), m_pBufferUV(0)
{
    m_pProgram = new ShaderProgramGL("shaders/gl/Vpx.vsh", "shaders/gl/Vpx.fsh");
#if GRIMROCK_HAVE_VPX
    debugPrint("Using %s\n", vpx_codec_iface_name(vpx_codec_vp8_dx()));
    vpx_codec_dec_cfg_t cfg = {0, 0, 0};
    if (vpx_codec_dec_init(&m_codec, vpx_codec_vp8_dx(), &cfg, 0))
        throw Exception("Failed to initialize vpx decoder");
#else
    debugPrint("Built without libvpx: videos are skipped\n");
#endif
}
// 0x004f2230
VPXPlayerGL::~VPXPlayerGL()
{
    close();
#if GRIMROCK_HAVE_VPX
    // the original throws here; a C++11 destructor is noexcept, so a throw would end the
    // program instead of reaching a handler
    if (vpx_codec_destroy(&m_codec))
        debugPrint("Failed to destroy vpx codec\n");
#endif
    delete m_pTextureV;
    delete m_pTextureU;
    delete m_pTextureY;
    delete m_pProgram;
}

// 0x004f1cb0
void VPXPlayerGL::open(const char* filename)
{
    close();
    m_pFile = openRead(filename);
    unsigned char header[IVFHeaderSize];
    m_pFile->read(header, IVFHeaderSize);
    if (memcmp(header, "DKIF", 4) != 0)
        throw Exception("Not a valid IVF file: %s", filename);
    m_width = header[12] | (header[13] << 8);
    m_height = header[14] | (header[15] << 8);
    unsigned int rate = header[16] | (header[17] << 8) | (header[18] << 16) | (header[19] << 24);
    unsigned int scale = header[20] | (header[21] << 8) | (header[22] << 16) | (header[23] << 24);
    if (scale < 2)
        scale = 1;
    m_frameRate = (float)rate / (float)scale;
    if (m_frameRate < 1.0f || m_frameRate > 120.0f)
        m_frameRate = DefaultFrameRate;
    if (!m_pTextureY || m_pTextureY->getWidth() != m_width || m_pTextureY->getHeight() != m_height)
    {
        delete m_pTextureY;
        delete m_pTextureU;
        delete m_pTextureV;
        m_pTextureY = new Texture2DGL(m_width, m_height, 1, GL_RED, GL_LINEAR, GL_LINEAR,
                                      GL_CLAMP_TO_EDGE, GL_RED);
        m_pTextureU = new Texture2DGL(m_width / 2, m_height / 2, 1, GL_RED, GL_LINEAR, GL_LINEAR,
                                      GL_CLAMP_TO_EDGE, GL_RED);
        m_pTextureV = new Texture2DGL(m_width / 2, m_height / 2, 1, GL_RED, GL_LINEAR, GL_LINEAR,
                                      GL_CLAMP_TO_EDGE, GL_RED);
    }
    m_timer.reset();
    m_frame = 0;
    delete[] m_pBufferY;
    delete[] m_pBufferUV;
    m_pBufferY = new unsigned char[m_width * m_height];
    m_pBufferUV = new unsigned char[(m_width / 2) * (m_height / 2)];
}
void VPXPlayerGL::close()
{
    if (m_pFile)
        closeFile(m_pFile);
    m_pFile = 0;
    delete[] m_pBufferY;
    delete[] m_pBufferUV;
    m_pBufferY = 0;
    m_pBufferUV = 0;
    m_frameData.clear();
}
// 0x004f1c00: done once the last frame header has been read
bool VPXPlayerGL::isDone()
{
#if !GRIMROCK_HAVE_VPX
    return true;
#endif
    return !m_pFile || m_pFile->getFileLength() - m_pFile->getFilePosition() < IVFFrameHeaderSize;
}
int VPXPlayerGL::getWidth()
{
    return m_width;
}
int VPXPlayerGL::getHeight()
{
    return m_height;
}

// 0x004f1c30
void VPXPlayerGL::uploadPlane(const unsigned char* data, int stride, Texture2DGL* texture,
                              int width, int height, unsigned char* buffer)
{
    unsigned char* dst = buffer;
    for (int y = 0; y < height; ++y)
    {
        memcpy(dst, data, width);
        data += stride;
        dst += width;
    }
    glBindTexture(GL_TEXTURE_2D, texture->getHandle());
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, buffer);
}

// 0x004f2360: decodes count frames, the last one lands in the textures
void VPXPlayerGL::decodeFrames(int count)
{
#if GRIMROCK_HAVE_VPX
    if (isDone())
        return;
    for (int i = 0; i < count && !isDone(); ++i)
    {
        unsigned char frameHeader[IVFFrameHeaderSize];
        m_pFile->read(frameHeader, IVFFrameHeaderSize);
        int size = frameHeader[0] | (frameHeader[1] << 8) | (frameHeader[2] << 16) |
                   (frameHeader[3] << 24);
        if (m_frameData.size() < size)
            m_frameData.resize(size);
        m_pFile->read(m_frameData.data(), size);
        if (vpx_codec_decode(&m_codec, m_frameData.data(), size, 0, 0))
            throw Exception("Failed to decode frame");
    }
    vpx_codec_iter_t iter = 0;
    vpx_image_t* image = 0;
    int frames = 0;
    for (vpx_image_t* img = vpx_codec_get_frame(&m_codec, &iter); img;
         img = vpx_codec_get_frame(&m_codec, &iter))
    {
        image = img;
        ++frames;
    }
    if (frames > 1)
        debugPrint("warning! %d vpx frames in 1\n", frames);
    if (!image)
        return;
    uploadPlane(image->planes[0], image->stride[0], m_pTextureY, m_width, m_height, m_pBufferY);
    uploadPlane(image->planes[1], image->stride[1], m_pTextureU, m_width / 2, m_height / 2,
                m_pBufferUV);
    uploadPlane(image->planes[2], image->stride[2], m_pTextureV, m_width / 2, m_height / 2,
                m_pBufferUV);
#endif
}

// 0x004f1f50
void VPXPlayerGL::render()
{
    Renderer* renderer = Renderer::getActiveRenderer();
    int width = renderer->m_viewportWidth, height = renderer->m_viewportHeight;
    glViewport(renderer->m_viewportX, renderer->m_viewportY, width, height);
    checkGLErrors("glViewport");
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    m_pContext->setBlendMode(RenderContextGL::Blend_Opaque);
    m_pContext->useProgram(m_pProgram);
    m_pContext->setUniformTexture("g_texY", m_pTextureY, Material::Linear, Material::Clamp, 0);
    m_pContext->setUniformTexture("g_texU", m_pTextureU, Material::Linear, Material::Clamp, 1);
    m_pContext->setUniformTexture("g_texV", m_pTextureV, Material::Linear, Material::Clamp, 2);
    m_pProgram->setUniform("g_texcoordOffset", Vec2(0.5f / (float)width, 0.5f / (float)height));
    m_pContext->drawRect();
}

// 0x004f2580
void VPXPlayerGL::update()
{
    if (!m_pFile || !m_pTextureY)
        return;
    int frame = (int)(m_timer.get() * m_frameRate) + 1;
    if (frame > m_frame)
    {
        decodeFrames(frame - m_frame);
        m_frame = frame;
    }
    render();
}

} // namespace engine
