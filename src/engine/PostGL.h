// Post processing filters, reconstructed from PostGL.cpp (0x08123cf0-0x08126c30).
#pragma once
#include "core/Vector.h"
#include "engine/RenderContextGL.h"

namespace engine
{

class Camera;

class ScreenSpaceAmbientOcclusionGL
{
  public:
    ScreenSpaceAmbientOcclusionGL(RenderContextGL* context, int width, int height);
    ~ScreenSpaceAmbientOcclusionGL();
    void resizeRenderBuffers(int width, int height);
    // Depth prepare, occlusion (high quality when quality > 0), separable blur and
    // modulation of the colour buffer.
    void render(const Camera& camera, Texture2DGL* geometryBuffer, Texture2DGL* colorBuffer,
                int quality);
    Texture2DGL* getResultBuffer() const
    {
        return m_pOcclusionBuffer;
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

class FogFilterGL
{
  public:
    FogFilterGL(RenderContextGL* context);
    ~FogFilterGL();
    void render(Texture2DGL* geometryBuffer, Texture2DGL* target);

    core::Vec3 m_color;
    float m_start;
    float m_end;

  private:
    RenderContextGL* m_pContext;
    ShaderProgramGL* m_pProgram;
};

class TonemapperGL
{
  public:
    TonemapperGL(RenderContextGL* context);
    ~TonemapperGL();
    // target 0 = window
    void render(Texture2DGL* source, Texture2DGL* target);

  private:
    RenderContextGL* m_pContext;
    ShaderProgramGL* m_pProgram;
};

} // namespace engine
