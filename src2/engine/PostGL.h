// Post processing filters of Legend of Grimrock 2, reconstructed from grimrock2.exe
// PostGL.cpp (0x004ef330-0x004f1bb0): ambient occlusion, fog with particles, tone mapping
// with saturation, water refraction and a separable blur.
#pragma once
#include "core/Vector.h"
#include "engine/RenderContextGL.h"
#include "engine/Renderer.h"

namespace engine
{

class Camera;

// 0x38 bytes; the settings come first so Lua's SSAOFilter is this object.
class ScreenSpaceAmbientOcclusionGL : public SSAOFilter
{
  public:
    // 0x004f0500
    ScreenSpaceAmbientOcclusionGL(RenderContextGL* context, int width, int height);
    ~ScreenSpaceAmbientOcclusionGL();
    // 0x004ef330
    void resizeRenderBuffers(int width, int height);
    // 0x004ef450: linear depth of the geometry buffer
    void prepareDepth(Texture2DGL* geometryBuffer);
    // 0x004f09e0: occlusion (high quality when quality > 0), separable blur and
    // modulation of the target.
    void render(const Camera& camera, Texture2DGL* geometryBuffer, Texture2DGL* target);
    Texture2DGL* getResultBuffer() const
    {
        return m_pOcclusionBuffer;
    }
    Texture2DGL* getDepthBuffer() const
    {
        return m_pDepthBuffer;
    }

  private:
    RenderContextGL* m_pContext;
    int m_width;
    int m_height;
    ShaderProgramGL* m_pPrepareDepthProgram;
    ShaderProgramGL* m_pOcclusionProgram;
    ShaderProgramGL* m_pOcclusionHQProgram;
    ShaderProgramGL* m_pBlurXProgram;
    ShaderProgramGL* m_pBlurYProgram;
    Texture2DGL* m_pDepthBuffer;     // R16F
    Texture2DGL* m_pOcclusionBuffer; // RGBA16F
    Texture2DGL* m_pBlurBuffer;      // RG16F
    Texture2DGL* m_pRotTexture;      // 4x4 rotation pattern
};

// 0x6c bytes
class FogFilterGL : public FogFilter
{
  public:
    // 0x004f1780
    FogFilterGL(RenderContextGL* context);
    ~FogFilterGL();
    // 0x004f13c0: blends the fog over the target by view depth in the selected mode,
    // then the fog particles.
    void render(Texture2DGL* geometryBuffer, Texture2DGL* target);

  private:
    // 0x004efa30: one camera facing quad per fog particle
    void renderParticles(Texture2DGL* geometryBuffer);
    RenderContextGL* m_pContext;
    GLuint m_vertexArray;
    ShaderProgramGL* m_pLinearProgram;
    ShaderProgramGL* m_pLinearLitProgram;
    ShaderProgramGL* m_pExpProgram;
    ShaderProgramGL* m_pDenseProgram;
    ShaderProgramGL* m_pParticleProgram;
};

// 0xc bytes
class TonemapperGL : public Tonemapper
{
  public:
    // 0x004f1280
    TonemapperGL(RenderContextGL* context);
    ~TonemapperGL();
    // 0x004ef550: target 0 = window
    void render(Texture2DGL* source, Texture2DGL* target);

  private:
    RenderContextGL* m_pContext;
    ShaderProgramGL* m_pProgram;
};

// 0x18 bytes
class WaterRefractionGL
{
  public:
    // 0x004f16a0
    WaterRefractionGL(RenderContextGL* context);
    ~WaterRefractionGL();
    // 0x004f02e0: the frame refracted below the clipping plane into the target
    void render(Texture2DGL* geometryBuffer, Texture2DGL* frameBuffer, Texture2DGL* target);
    void setClippingPlane(const core::Vec4& plane)
    {
        m_clippingPlane = plane;
    }

  private:
    RenderContextGL* m_pContext;
    ShaderProgramGL* m_pProgram;
    core::Vec4 m_clippingPlane;
};

// 8 bytes: 7 tap box blur in two passes.
class BlurGL
{
  public:
    // 0x004f1320
    BlurGL(RenderContextGL* context);
    ~BlurGL();
    // 0x004ef680: blurs image in place through temp
    void blur(Texture2DGL* image, Texture2DGL* temp);

  private:
    RenderContextGL* m_pContext;
    ShaderProgramGL* m_pProgram;
};

} // namespace engine
