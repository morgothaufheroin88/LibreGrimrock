// Reconstructed from Grimrock.bin.x86 PostGL.cpp.
#include "engine/PostGL.h"
#include "core/Math.h"
#include "engine/Camera.h"
#include <cmath>

namespace engine
{

using namespace core;

static void setProgram(ShaderProgramGL*& slot, ShaderProgramGL* program)
{
    if (slot != program)
    {
        delete slot;
        slot = program;
    }
}
static void setTexture2D(Texture2DGL*& slot, Texture2DGL* texture)
{
    if (slot != texture)
    {
        delete slot;
        slot = texture;
    }
}
// SSAO kernel radius in pixels and the depth difference at which the blur stops mixing.
constexpr float SampleRadiusPixels = 35.0f;
constexpr float BlurDepthThreshold = 0.02f;

static unsigned char toByte(float v)
{
    int i = (int)lrintf(v * 255.0f);
    return (unsigned char)(i > 255 ? 255 : (i < 0 ? 0 : i));
}

// ---- ScreenSpaceAmbientOcclusionGL -----------------------------------------------

// 0x081244b0
ScreenSpaceAmbientOcclusionGL::ScreenSpaceAmbientOcclusionGL(RenderContextGL* context, int width,
                                                             int height)
    : m_pContext(context), m_width(0), m_height(0), m_pPrepareDepthProgram(0),
      m_pOcclusionProgram(0), m_pOcclusionHQProgram(0), m_pBlurXProgram(0), m_pBlurYProgram(0),
      m_pDepthBuffer(0), m_pOcclusionBuffer(0), m_pBlurBuffer(0), m_pRotTexture(0)
{
    resizeRenderBuffers(width, height);
    setProgram(m_pPrepareDepthProgram,
               new ShaderProgramGL("shaders/gl/AmbientOcclusionPrepareDepth.vsh",
                                   "shaders/gl/AmbientOcclusionPrepareDepth.fsh"));
    setProgram(m_pOcclusionProgram, new ShaderProgramGL("shaders/gl/AmbientOcclusion.vsh",
                                                        "shaders/gl/AmbientOcclusion.fsh"));
    static constexpr const char* hqDefines[] = {"USE_HIGH_QUALITY", 0};
    GLuint vs = RenderContextGL::compileShaderFromFile("shaders/gl/AmbientOcclusion.vsh",
                                                       GL_VERTEX_SHADER, 0);
    GLuint fs = RenderContextGL::compileShaderFromFile("shaders/gl/AmbientOcclusion.fsh",
                                                       GL_FRAGMENT_SHADER, hqDefines);
    setProgram(m_pOcclusionHQProgram, new ShaderProgramGL(vs, fs));
    setProgram(m_pBlurXProgram, new ShaderProgramGL("shaders/gl/AmbientOcclusionBlur.vsh",
                                                    "shaders/gl/AmbientOcclusionBlurX.fsh"));
    setProgram(m_pBlurYProgram, new ShaderProgramGL("shaders/gl/AmbientOcclusionBlur.vsh",
                                                    "shaders/gl/AmbientOcclusionBlurY.fsh"));

    // 4x4 texture of rotations in a dither pattern, (cos, -sin, sin, cos) per texel
    constexpr int NumRotations = 16;
    static constexpr int pattern[NumRotations] = {1, 9,  3, 11, 13, 5, 15, 7,
                                                  4, 12, 2, 10, 16, 8, 14, 6};
    unsigned char pixels[NumRotations][4] = {{0xff, 0x7f, 0x7f, 0xff}}; // angle 0 = (1, 0, 0, 1)
    for (int i = 1; i < NumRotations; ++i)
    {
        float angle = (float)(pattern[i] - 1) * PI * 2.0f / NumRotations;
        float sinA = std::sin(angle), cosA = std::cos(angle);
        pixels[i][0] = toByte(cosA * 0.5f + 0.5f);
        pixels[i][1] = toByte(-sinA * 0.5f + 0.5f);
        pixels[i][2] = toByte(sinA * 0.5f + 0.5f);
        pixels[i][3] = toByte(cosA * 0.5f + 0.5f);
    }
    setTexture2D(m_pRotTexture,
                 new Texture2DGL(4, 4, 1, GL_RGBA8, GL_NEAREST, GL_NEAREST, GL_REPEAT));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
    checkGLErrors("glTexImage2D");
}

ScreenSpaceAmbientOcclusionGL::~ScreenSpaceAmbientOcclusionGL()
{
    delete m_pRotTexture;
    delete m_pBlurBuffer;
    delete m_pOcclusionBuffer;
    delete m_pDepthBuffer;
    delete m_pBlurYProgram;
    delete m_pBlurXProgram;
    delete m_pOcclusionHQProgram;
    delete m_pOcclusionProgram;
    delete m_pPrepareDepthProgram;
}

// 0x081240f0
void ScreenSpaceAmbientOcclusionGL::resizeRenderBuffers(int width, int height)
{
    m_width = width;
    m_height = height;
    setTexture2D(m_pDepthBuffer, new Texture2DGL(width, height, 1, GL_R16F, GL_NEAREST, GL_NEAREST,
                                                 GL_CLAMP_TO_EDGE));
    setTexture2D(m_pOcclusionBuffer, new Texture2DGL(width, height, 1, GL_RGBA16F, GL_NEAREST,
                                                     GL_NEAREST, GL_CLAMP_TO_EDGE));
    setTexture2D(m_pBlurBuffer, new Texture2DGL(width, height, 1, GL_RG16F, GL_NEAREST, GL_NEAREST,
                                                GL_CLAMP_TO_EDGE));
}

// 0x08125c40
void ScreenSpaceAmbientOcclusionGL::render(const Camera& camera, Texture2DGL* geometryBuffer,
                                           Texture2DGL* colorBuffer, int quality)
{
    constexpr int NumSamples = 17;
    static constexpr float samples[NumSamples][2] = {
        {-0.167238f, -0.58778203f},  {0.65689701f, 0.59884101f},
        {0.247632f, -0.49731299f},   {-0.091220997f, 0.320609f},
        {-0.218438f, -0.040833f},    {-0.23617101f, 0.146231f},
        {0.076889999f, 0.82977802f}, {0.173343f, 0.34811899f},
        {0.466236f, -0.180621f},     {0.41443199f, 0.160552f},
        {-0.709925f, 0.132708f},     {0.010252f, -0.110637f},
        {0.944444f, 0.0f},           {-0.468716f, 0.62067997f},
        {0.041056f, -0.037427999f},  {-0.56681103f, -0.35095501f},
        {-0.100439f, -0.133003f}};
    const Matrix4x4& invProj = camera.getInverseProjectionMatrix();
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    m_pContext->setBlendMode(RenderContextGL::Blend_Opaque);
    glDisable(GL_ALPHA_TEST);

    // linear depth
    m_pContext->setRenderTarget(m_pDepthBuffer, 0, 0, 0);
    m_pContext->useProgram(m_pPrepareDepthProgram);
    m_pContext->setUniformTexture("geometryBuffer", geometryBuffer, -1, -1);
    m_pPrepareDepthProgram->setUniform("invScreenSize", Vec2(1.0f / m_width, 1.0f / m_height));
    m_pContext->drawRect();

    // occlusion
    m_pContext->setRenderTarget(m_pOcclusionBuffer, 0, 0, 0);
    ShaderProgramGL* prog = quality > 0 ? m_pOcclusionHQProgram : m_pOcclusionProgram;
    m_pContext->useProgram(prog);
    m_pContext->setUniformTexture("geometryBuffer", geometryBuffer, -1, -1);
    m_pContext->setUniformTexture("depthBuffer", m_pDepthBuffer, -1, -1);
    m_pContext->setUniformTexture("rotTex", m_pRotTexture, -1, -1);
    prog->setUniform("invProjectionMatrix", invProj);
    prog->setUniform("invNear", 1.0f / camera.getNear());
    prog->setUniform2v("samples", &samples[0][0], NumSamples);
    prog->setUniform("pixRadius", SampleRadiusPixels);
    // view space size of a pixel at unit depth, from the near plane corners
    Vec4 topLeft = invProj.transform(Vec4(-1, 1, 0, 1));
    Vec4 bottomRight = invProj.transform(Vec4(1, -1, 0, 1));
    float ax = topLeft.x / topLeft.w / (topLeft.z / topLeft.w);
    float ay = topLeft.y / topLeft.w / (topLeft.z / topLeft.w);
    float bx = bottomRight.x / bottomRight.w / (bottomRight.z / bottomRight.w);
    float by = bottomRight.y / bottomRight.w / (bottomRight.z / bottomRight.w);
    prog->setUniform("screenToView", Vec2((bx - ax) / m_width, (ay - by) / m_height));
    prog->setUniform("invScreenSize", Vec2(1.0f / m_width, 1.0f / m_height));
    m_pContext->drawRect();

    // separable blur, the second pass modulates the colour buffer
    float w = (float)m_width, h = (float)m_height;
    float offsetX[8] = {-2.0f / w, 0, -1.0f / w, 0, 1.0f / w, 0, 2.0f / w, 0};
    float offsetY[8] = {0, -2.0f / h, 0, -1.0f / h, 0, 1.0f / h, 0, 2.0f / h};
    m_pContext->setRenderTarget(m_pBlurBuffer, 0, 0, 0);
    m_pContext->useProgram(m_pBlurXProgram);
    m_pContext->setUniformTexture("ssaoBuffer", m_pOcclusionBuffer, -1, -1);
    m_pBlurXProgram->setUniform("threshold", BlurDepthThreshold);
    m_pBlurXProgram->setUniform2v("offset", offsetX, 4);
    m_pBlurXProgram->setUniform("invScreenSize", Vec2(1.0f / m_width, 1.0f / m_height));
    m_pContext->drawRect();

    m_pContext->setRenderTarget(colorBuffer, 0, 0, 0);
    m_pContext->useProgram(m_pBlurYProgram);
    m_pContext->setUniformTexture("ssaoBuffer", m_pBlurBuffer, -1, -1);
    m_pBlurYProgram->setUniform("threshold", BlurDepthThreshold);
    m_pBlurYProgram->setUniform2v("offset", offsetY, 4);
    m_pBlurYProgram->setUniform("invScreenSize", Vec2(1.0f / m_width, 1.0f / m_height));
    m_pContext->setBlendMode(RenderContextGL::Blend_Modulative);
    m_pContext->drawRect();
}

// ---- FogFilterGL -----------------------------------------------------------------

// 0x08123d20
FogFilterGL::FogFilterGL(RenderContextGL* context)
    : m_color(0, 0, 0), m_start(0), m_end(0), m_pContext(context), m_pProgram(0)
{
    m_pProgram = new ShaderProgramGL("shaders/gl/Fog.vsh", "shaders/gl/Fog.fsh");
}
FogFilterGL::~FogFilterGL()
{
    delete m_pProgram;
}

// 0x08125920: blends the fog colour over the target by view depth.
void FogFilterGL::render(Texture2DGL* geometryBuffer, Texture2DGL* target)
{
    m_pContext->setRenderTarget(target, 0, 0, 0);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_ALPHA_TEST);
    m_pContext->setBlendMode(RenderContextGL::Blend_Translucent);
    m_pContext->useProgram(m_pProgram);
    m_pContext->setUniformTexture("geometryBuffer", geometryBuffer, -1, -1);
    m_pProgram->setUniform("invScreenSize", Vec2(1.0f / geometryBuffer->getWidth(),
                                                 1.0f / geometryBuffer->getHeight()));
    m_pProgram->setUniform("fogColor", m_color);
    float range = m_end - m_start;
    m_pProgram->setUniform("fogParams", Vec2(1.0f / range, -m_start / range));
    m_pContext->drawRect();
}

// ---- TonemapperGL ----------------------------------------------------------------

// 0x08123dd0
TonemapperGL::TonemapperGL(RenderContextGL* context) : m_pContext(context), m_pProgram(0)
{
    m_pProgram = new ShaderProgramGL("shaders/gl/Tonemap.vsh", "shaders/gl/Tonemap.fsh");
}
TonemapperGL::~TonemapperGL()
{
    delete m_pProgram;
}

// 0x08123e60
void TonemapperGL::render(Texture2DGL* source, Texture2DGL* target)
{
    if (!target)
        m_pContext->setRenderTarget(RenderContextGL::DefaultFrameBuffer);
    else
        m_pContext->setRenderTarget(target, 0, 0, 0);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_CULL_FACE);
    m_pContext->setBlendMode(RenderContextGL::Blend_Opaque);
    m_pContext->useProgram(m_pProgram);
    m_pContext->setUniformTexture("sourceTex", source, -1, -1);
    m_pProgram->setUniform("invScreenSize",
                           Vec2(1.0f / source->getWidth(), 1.0f / source->getHeight()));
    m_pContext->drawRect();
}

} // namespace engine
