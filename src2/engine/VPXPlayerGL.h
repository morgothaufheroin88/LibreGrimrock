// VP8 cinematic player of Legend of Grimrock 2, reconstructed from grimrock2.exe
// VPXPlayerGL.cpp (0x004f1bb0-0x004f2580): IVF frames decoded with libvpx into three
// planar textures and drawn through shaders/gl/Vpx.
#pragma once
#include "core/Array.h"
#include "core/Timer.h"
#include "engine/RenderContextGL.h"
#include "engine/Renderer.h"

#if GRIMROCK_HAVE_VPX
#include <vpx/vpx_decoder.h>
#endif

namespace core
{
class File;
}

namespace engine
{

// 0x70 bytes
class VPXPlayerGL : public VPXPlayer
{
  public:
    // 0x004f20f0
    VPXPlayerGL(RenderContextGL* context);
    // 0x004f2230
    ~VPXPlayerGL();
    // 0x004f1cb0: reads the IVF header and allocates the plane textures
    void open(const char* filename);
    // 0x004f2230 (inner)
    void close();
    // 0x004f2580: decodes the frames due since the last update and draws the latest
    void update();
    // 0x004f1c00
    bool isDone();
    int getWidth();
    int getHeight();

  private:
    // 0x004f2360
    void decodeFrames(int count);
    // 0x004f1f50: the Y, U and V planes to the viewport of the renderer
    void render();
    // 0x004f1c30
    void uploadPlane(const unsigned char* data, int stride, Texture2DGL* texture, int width,
                     int height, unsigned char* buffer);

    RenderContextGL* m_pContext;
    ShaderProgramGL* m_pProgram;
    core::File* m_pFile;
#if GRIMROCK_HAVE_VPX
    vpx_codec_ctx_t m_codec;
#endif
    core::Array<unsigned char> m_frameData;
    Texture2DGL* m_pTextureY;
    Texture2DGL* m_pTextureU;
    Texture2DGL* m_pTextureV;
    int m_width;
    int m_height;
    float m_frameRate;
    int m_frame;
    core::Timer m_timer;
    unsigned char* m_pBufferY;
    unsigned char* m_pBufferUV;
};

} // namespace engine
