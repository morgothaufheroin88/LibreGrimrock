// Reconstructed from grimrock2.exe PostGL.cpp.
#include "engine/PostGL.h"
#include "core/Math.h"
#include "engine/Camera.h"
#include "engine/RendererGL.h"
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
constexpr int NumBlurTaps = 7;

static unsigned char toByte(float v)
{
    int i = (int)lrintf(v * 255.0f);
    return (unsigned char)(i > 255 ? 255 : (i < 0 ? 0 : i));
}

// ---- ScreenSpaceAmbientOcclusionGL -----------------------------------------------

// 0x004f0500
ScreenSpaceAmbientOcclusionGL::ScreenSpaceAmbientOcclusionGL(RenderContextGL* context, int width,
                                                             int height)
    : m_pContext(context), m_width(0), m_height(0), m_pPrepareDepthProgram(0),
      m_pOcclusionProgram(0), m_pOcclusionHQProgram(0), m_pBlurXProgram(0), m_pBlurYProgram(0),
      m_pDepthBuffer(0), m_pOcclusionBuffer(0), m_pBlurBuffer(0), m_pRotTexture(0)
{
    quality = 1;
    intensity = 1.0f;
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
    glDeleteShader(vs);
    glDeleteShader(fs);
    setProgram(m_pBlurXProgram, new ShaderProgramGL("shaders/gl/AmbientOcclusionBlur.vsh",
                                                    "shaders/gl/AmbientOcclusionBlurX.fsh"));
    setProgram(m_pBlurYProgram, new ShaderProgramGL("shaders/gl/AmbientOcclusionBlur.vsh",
                                                    "shaders/gl/AmbientOcclusionBlurY.fsh"));

    // 4x4 texture of rotations in a dither pattern, (cos, -sin, sin, cos) per texel
    constexpr int NumRotations = 16;
    static constexpr int pattern[NumRotations] = {1, 9,  3, 11, 13, 5, 15, 7,
                                                  4, 12, 2, 10, 16, 8, 14, 6};
    unsigned char pixels[NumRotations][4];
    for (int i = 0; i < NumRotations; ++i)
    {
        float angle = (float)(pattern[i] - 1) * PI * 2.0f / NumRotations;
        float sinA = std::sin(angle), cosA = std::cos(angle);
        pixels[i][0] = toByte(cosA * 0.5f + 0.5f);
        pixels[i][1] = toByte(-sinA * 0.5f + 0.5f);
        pixels[i][2] = toByte(sinA * 0.5f + 0.5f);
        pixels[i][3] = toByte(cosA * 0.5f + 0.5f);
    }
    setTexture2D(m_pRotTexture,
                 new Texture2DGL(4, 4, 1, GL_RGBA8, GL_NEAREST, GL_NEAREST, GL_REPEAT, GL_RGBA));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
    checkGLErrors("glTexImage2D");
}
// 0x004ef2a0
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
// 0x004ef330
void ScreenSpaceAmbientOcclusionGL::resizeRenderBuffers(int width, int height)
{
    m_width = width;
    m_height = height;
    setTexture2D(m_pDepthBuffer, new Texture2DGL(width, height, 1, GL_R16F, GL_NEAREST, GL_NEAREST,
                                                 GL_CLAMP_TO_EDGE, GL_RGBA));
    setTexture2D(m_pOcclusionBuffer, new Texture2DGL(width, height, 1, GL_RGBA16F, GL_NEAREST,
                                                     GL_NEAREST, GL_CLAMP_TO_EDGE, GL_RGBA));
    setTexture2D(m_pBlurBuffer, new Texture2DGL(width, height, 1, GL_RG16F, GL_NEAREST, GL_NEAREST,
                                                GL_CLAMP_TO_EDGE, GL_RGBA));
}
// 0x004ef450
void ScreenSpaceAmbientOcclusionGL::prepareDepth(Texture2DGL* geometryBuffer)
{
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    m_pContext->setBlendMode(RenderContextGL::Blend_Opaque);
    glDisable(GL_CULL_FACE);
    m_pContext->setRenderTarget(m_pDepthBuffer, 0, 0, 0);
    m_pContext->useProgram(m_pPrepareDepthProgram);
    m_pContext->setUniformTexture("g_geometryBuffer", geometryBuffer, -1, -1, 0);
    glUniform2f(glGetUniformLocation(m_pPrepareDepthProgram->getProgram(), "g_invScreenSize"),
                1.0f / m_width, 1.0f / m_height);
    m_pContext->drawRect();
}
// 0x004f09e0
void ScreenSpaceAmbientOcclusionGL::render(const Camera& camera, Texture2DGL* geometryBuffer,
                                           Texture2DGL* target)
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
    // view space size of a pixel at unit depth, from the near plane corners
    Vec4 topLeft = invProj.transform(Vec4(-1, 1, 0, 1));
    Vec4 bottomRight = invProj.transform(Vec4(1, -1, 0, 1));
    float ax = topLeft.x / topLeft.w / (topLeft.z / topLeft.w);
    float ay = topLeft.y / topLeft.w / (topLeft.z / topLeft.w);
    float bx = bottomRight.x / bottomRight.w / (bottomRight.z / bottomRight.w);
    float by = bottomRight.y / bottomRight.w / (bottomRight.z / bottomRight.w);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    m_pContext->setBlendMode(RenderContextGL::Blend_Opaque);
    glDisable(GL_CULL_FACE);

    // occlusion
    m_pContext->setRenderTarget(m_pOcclusionBuffer, 0, 0, 0);
    ShaderProgramGL* prog = quality > 0 ? m_pOcclusionHQProgram : m_pOcclusionProgram;
    m_pContext->useProgram(prog);
    GLuint program = prog->getProgram();
    m_pContext->setUniformTexture("g_geometryBuffer", geometryBuffer, -1, -1, 0);
    m_pContext->setUniformTexture("g_depthBuffer", m_pDepthBuffer, -1, -1, 1);
    m_pContext->setUniformTexture("g_rotTex", m_pRotTexture, -1, -1, 2);
    glUniformMatrix4fv(glGetUniformLocation(program, "g_invProjectionMatrix"), 1, GL_FALSE,
                       invProj.m);
    glUniform1f(glGetUniformLocation(program, "g_invNear"), 1.0f / camera.getNear());
    glUniform2fv(glGetUniformLocation(program, "g_samples"), NumSamples, &samples[0][0]);
    glUniform1f(glGetUniformLocation(program, "g_pixRadius"), SampleRadiusPixels);
    glUniform2f(glGetUniformLocation(program, "g_screenToView"), (bx - ax) / m_width,
                (ay - by) / m_height);
    glUniform2f(glGetUniformLocation(program, "g_invScreenSize"), 1.0f / m_width, 1.0f / m_height);
    glUniform1f(glGetUniformLocation(program, "g_ssaoIntensity"), intensity + intensity);
    m_pContext->drawRect();

    // separable blur, the second pass modulates the target
    float w = (float)m_width, h = (float)m_height;
    float offsetX[8] = {-2.0f / w, 0, -1.0f / w, 0, 1.0f / w, 0, 2.0f / w, 0};
    float offsetY[8] = {0, -2.0f / h, 0, -1.0f / h, 0, 1.0f / h, 0, 2.0f / h};
    m_pContext->setRenderTarget(m_pBlurBuffer, 0, 0, 0);
    m_pContext->useProgram(m_pBlurXProgram);
    m_pContext->setUniformTexture("g_ssaoBuffer", m_pOcclusionBuffer, -1, -1, 0);
    glUniform1f(glGetUniformLocation(m_pBlurXProgram->getProgram(), "g_threshold"),
                BlurDepthThreshold);
    glUniform2fv(glGetUniformLocation(m_pBlurXProgram->getProgram(), "g_offset"), 4, offsetX);
    glUniform2f(glGetUniformLocation(m_pBlurXProgram->getProgram(), "g_invScreenSize"),
                1.0f / m_width, 1.0f / m_height);
    m_pContext->drawRect();

    m_pContext->setRenderTarget(target, 0, 0, 0);
    m_pContext->useProgram(m_pBlurYProgram);
    m_pContext->setUniformTexture("g_ssaoBuffer", m_pBlurBuffer, -1, -1, 0);
    glUniform1f(glGetUniformLocation(m_pBlurYProgram->getProgram(), "g_threshold"),
                BlurDepthThreshold);
    glUniform2fv(glGetUniformLocation(m_pBlurYProgram->getProgram(), "g_offset"), 4, offsetY);
    glUniform2f(glGetUniformLocation(m_pBlurYProgram->getProgram(), "g_invScreenSize"),
                1.0f / m_width, 1.0f / m_height);
    m_pContext->setBlendMode(RenderContextGL::Blend_Modulative);
    m_pContext->drawRect();
}

// ---- FogFilterGL -----------------------------------------------------------------

// 0x004f1780: one fragment program per fog mode
FogFilterGL::FogFilterGL(RenderContextGL* context)
    : m_pContext(context), m_vertexArray(0), m_pLinearProgram(0), m_pLinearLitProgram(0),
      m_pExpProgram(0), m_pDenseProgram(0), m_pParticleProgram(0)
{
    static constexpr const char* linearDefines[] = {"FOG_LINEAR", 0};
    static constexpr const char* linearLitDefines[] = {"FOG_LINEAR_LIT", 0};
    static constexpr const char* expDefines[] = {"FOG_EXP", 0};
    static constexpr const char* denseDefines[] = {"FOG_DENSE", 0};
    GLuint vs = RenderContextGL::compileShaderFromFile("shaders/gl/Fog.vsh", GL_VERTEX_SHADER, 0);
    GLuint linear = RenderContextGL::compileShaderFromFile("shaders/gl/Fog.fsh", GL_FRAGMENT_SHADER,
                                                           linearDefines);
    GLuint linearLit = RenderContextGL::compileShaderFromFile("shaders/gl/Fog.fsh",
                                                              GL_FRAGMENT_SHADER, linearLitDefines);
    GLuint exp = RenderContextGL::compileShaderFromFile("shaders/gl/Fog.fsh", GL_FRAGMENT_SHADER,
                                                        expDefines);
    GLuint dense = RenderContextGL::compileShaderFromFile("shaders/gl/Fog.fsh", GL_FRAGMENT_SHADER,
                                                          denseDefines);
    setProgram(m_pLinearProgram, new ShaderProgramGL(vs, linear));
    setProgram(m_pLinearLitProgram, new ShaderProgramGL(vs, linearLit));
    setProgram(m_pExpProgram, new ShaderProgramGL(vs, exp));
    setProgram(m_pDenseProgram, new ShaderProgramGL(vs, dense));
    glDeleteShader(vs);
    glDeleteShader(linear);
    glDeleteShader(linearLit);
    glDeleteShader(exp);
    glDeleteShader(dense);
    setProgram(m_pParticleProgram,
               new ShaderProgramGL("shaders/gl/FogParticle.vsh", "shaders/gl/FogParticle.fsh"));
    glGenVertexArrays(1, &m_vertexArray);
    glBindVertexArray(m_vertexArray);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(4);
    glBindVertexArray(0);
}
// 0x004f1b00
FogFilterGL::~FogFilterGL()
{
    delete m_pParticleProgram;
    delete m_pDenseProgram;
    delete m_pExpProgram;
    delete m_pLinearLitProgram;
    delete m_pLinearProgram;
    glDeleteVertexArrays(1, &m_vertexArray);
}
// 0x004f13c0
void FogFilterGL::render(Texture2DGL* geometryBuffer, Texture2DGL* target)
{
    m_pContext->setRenderTarget(target, 0, 0, 0);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    m_pContext->setBlendMode(RenderContextGL::Blend_Translucent);
    ShaderProgramGL* prog;
    switch (m_fogMode)
    {
    case Fog_Linear:
        prog = m_pLinearProgram;
        break;
    case Fog_Exp:
        prog = m_pExpProgram;
        break;
    case Fog_Dense:
        prog = m_pDenseProgram;
        break;
    default:
        prog = m_pLinearLitProgram;
        break;
    }
    m_pContext->useProgram(prog);
    GLuint program = prog->getProgram();
    const Camera& camera = *m_pContext->getCamera();
    m_pContext->setUniformTexture("g_depthBuffer", geometryBuffer, -1, -1, 0);
    glUniformMatrix4fv(glGetUniformLocation(program, "g_invProjectionMatrix"), 1, GL_FALSE,
                       camera.getInverseProjectionMatrix().m);
    glUniform1f(glGetUniformLocation(program, "g_invNear"), 1.0f / camera.getNear());
    glUniform1f(glGetUniformLocation(program, "g_far"), camera.getFar());
    glUniform2f(glGetUniformLocation(program, "g_invScreenSize"), 1.0f / geometryBuffer->getWidth(),
                1.0f / geometryBuffer->getHeight());
    glUniform3f(glGetUniformLocation(program, "g_fogColor"), m_fogColor.x, m_fogColor.y,
                m_fogColor.z);
    float range = m_fogRange.y - m_fogRange.x;
    glUniform2f(glGetUniformLocation(program, "g_fogParams"), 1.0f / range, -m_fogRange.x / range);
    glUniform1f(glGetUniformLocation(program, "g_fogDensity"), -m_fogDensity);
    // light direction in view space
    Vec3 dir = camera.getWorldToLocalMatrix().rotation().transform(m_fogLightDirection);
    glUniform3f(glGetUniformLocation(program, "g_lightDirection"), dir.x, dir.y, dir.z);
    m_pContext->drawRect();
    if (m_particles.size() > 0 && m_particleSize > 0.0f)
        renderParticles(geometryBuffer);
}
// 28 bytes: position, corner, size, phase
struct FogParticleVertex
{
    Vec3 pos;
    float u, v;
    float size;
    float phase;
};
// 0x004efa30
void FogFilterGL::renderParticles(Texture2DGL* geometryBuffer)
{
    m_pContext->useProgram(m_pParticleProgram);
    GLuint program = m_pParticleProgram->getProgram();
    m_pContext->setBlendMode(RenderContextGL::Blend_AdditiveSrcAlpha);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    const Camera& camera = *m_pContext->getCamera();
    Matrix4x4 view(camera.getWorldToLocalMatrix());
    glUniformMatrix4fv(glGetUniformLocation(program, "g_modelView"), 1, GL_FALSE, view.m);
    Matrix4x4 proj = RenderContextGL::sm_d3dToGLProj * camera.getProjectionMatrix();
    glUniformMatrix4fv(glGetUniformLocation(program, "g_proj"), 1, GL_FALSE, proj.m);
    glUniform2f(glGetUniformLocation(program, "g_invScreenSize"), 1.0f / geometryBuffer->getWidth(),
                1.0f / geometryBuffer->getHeight());
    glUniform1f(glGetUniformLocation(program, "g_particleSize"), m_particleSize);
    glUniform3f(glGetUniformLocation(program, "g_particleColor"), m_particleColor.x,
                m_particleColor.y, m_particleColor.z);
    m_pContext->setUniformTexture("g_depthBuffer", geometryBuffer, -1, -1, 0);
    RenderableTextureGL* texture = (RenderableTextureGL*)m_particleTexture.get();
    if (!texture)
        texture = CommonResourcesGL::WhiteMap;
    m_pContext->setUniformTexture("g_texture", texture->getTexture(), -1, -1, 1);
    glBindVertexArray(m_vertexArray);
    int count = m_particles.size();
    int bytes = count * 4 * (int)sizeof(FogParticleVertex);
    FogParticleVertex* out = (FogParticleVertex*)m_pContext->streamWrite(bytes);
    if (!out)
        return;
    const char* base = (const char*)(intptr_t)(m_pContext->getStreamOffset() - bytes);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(FogParticleVertex), base);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(FogParticleVertex), base + 12);
    static constexpr float corners[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (int i = 0; i < count; ++i)
    {
        const Particle& p = m_particles[i];
        for (int c = 0; c < 4; ++c)
        {
            out->pos.set(p.x, p.y, p.z);
            out->u = corners[c][0];
            out->v = corners[c][1];
            out->size = p.size;
            out->phase = p.phase;
            ++out;
        }
    }
    m_pContext->streamDrawPrimitive(RenderContextGL::Prim_Quads, count * 4);
}

// ---- TonemapperGL ----------------------------------------------------------------

// 0x004f1280
TonemapperGL::TonemapperGL(RenderContextGL* context) : m_pContext(context), m_pProgram(0)
{
    saturation = 1.0f;
    setProgram(m_pProgram, new ShaderProgramGL("shaders/gl/Tonemap.vsh", "shaders/gl/Tonemap.fsh"));
}
TonemapperGL::~TonemapperGL()
{
    delete m_pProgram;
}
// 0x004ef550
void TonemapperGL::render(Texture2DGL* source, Texture2DGL* target)
{
    if (!target)
        m_pContext->setRenderTarget(RenderContextGL::DefaultFrameBuffer);
    else
        m_pContext->setRenderTarget(target, 0, 0, 0);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    m_pContext->setBlendMode(RenderContextGL::Blend_Opaque);
    m_pContext->useProgram(m_pProgram);
    m_pContext->setUniformTexture("g_sourceTex", source, -1, -1, 0);
    glUniform2f(glGetUniformLocation(m_pProgram->getProgram(), "g_invScreenSize"),
                1.0f / source->getWidth(), 1.0f / source->getHeight());
    glUniform1f(glGetUniformLocation(m_pProgram->getProgram(), "g_saturation"), saturation);
    m_pContext->drawRect();
}

// ---- WaterRefractionGL -----------------------------------------------------------

// 0x004f16a0
WaterRefractionGL::WaterRefractionGL(RenderContextGL* context)
    : m_pContext(context), m_pProgram(0), m_clippingPlane(0, 1, 0, 0)
{
    setProgram(m_pProgram, new ShaderProgramGL("shaders/gl/WaterRefraction.vsh",
                                               "shaders/gl/WaterRefraction.fsh"));
}
WaterRefractionGL::~WaterRefractionGL()
{
    delete m_pProgram;
}
// 0x004f02e0
void WaterRefractionGL::render(Texture2DGL* geometryBuffer, Texture2DGL* frameBuffer,
                               Texture2DGL* target)
{
    m_pContext->setRenderTarget(target, 0, 0, 0);
    m_pContext->useProgram(m_pProgram);
    GLuint program = m_pProgram->getProgram();
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    m_pContext->setBlendMode(RenderContextGL::Blend_Opaque);
    const Camera& camera = *m_pContext->getCamera();
    m_pContext->setUniformTexture("g_frameBuffer", frameBuffer, -1, -1, 0);
    m_pContext->setUniformTexture("g_geometryBuffer", geometryBuffer, -1, -1, 1);
    glUniformMatrix4fv(glGetUniformLocation(program, "g_invProjectionMatrix"), 1, GL_FALSE,
                       camera.getInverseProjectionMatrix().m);
    glUniform1f(glGetUniformLocation(program, "g_invNear"), 1.0f / camera.getNear());
    Matrix4x4 viewToWorld(camera.getLocalToWorldMatrix());
    glUniformMatrix4fv(glGetUniformLocation(program, "g_viewToWorldMatrix"), 1, GL_FALSE,
                       viewToWorld.m);
    glUniform4f(glGetUniformLocation(program, "g_clippingPlane"), m_clippingPlane.x,
                m_clippingPlane.y, m_clippingPlane.z, m_clippingPlane.w);
    glUniform2f(glGetUniformLocation(program, "g_invScreenSize"), 1.0f / geometryBuffer->getWidth(),
                1.0f / geometryBuffer->getHeight());
    m_pContext->drawRect();
}

// ---- BlurGL ----------------------------------------------------------------------

// 0x004f1320
BlurGL::BlurGL(RenderContextGL* context) : m_pContext(context), m_pProgram(0)
{
    setProgram(m_pProgram, new ShaderProgramGL("shaders/gl/Blur.vsh", "shaders/gl/Blur.fsh"));
}
BlurGL::~BlurGL()
{
    delete m_pProgram;
}
// 0x004ef680: horizontal pass into temp, vertical pass back into the image; the taps
// are (offset x, offset y, weight 1/7).
void BlurGL::blur(Texture2DGL* image, Texture2DGL* temp)
{
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);
    m_pContext->setBlendMode(RenderContextGL::Blend_Opaque);
    m_pContext->useProgram(m_pProgram);
    GLuint program = m_pProgram->getProgram();
    for (int pass = 0; pass < 2; ++pass)
    {
        Texture2DGL* target = pass == 0 ? temp : image;
        Texture2DGL* source = pass == 0 ? image : temp;
        m_pContext->setRenderTarget(target, 0, 0, 0);
        float taps[NumBlurTaps][3];
        for (int i = 0; i < NumBlurTaps; ++i)
        {
            float offset = (float)(i - NumBlurTaps / 2);
            taps[i][0] = pass == 0 ? offset / image->getWidth() : 0.0f;
            taps[i][1] = pass == 0 ? 0.0f : offset / image->getHeight();
            taps[i][2] = 1.0f / NumBlurTaps;
        }
        m_pContext->setUniformTexture("g_tex", source, Material::Linear, Material::Clamp, 0);
        glUniform2f(glGetUniformLocation(program, "g_invTexRes"), 1.0f / image->getWidth(),
                    1.0f / image->getHeight());
        glUniform3fv(glGetUniformLocation(program, "g_samples"), NumBlurTaps, &taps[0][0]);
        m_pContext->drawRect();
    }
}

} // namespace engine
