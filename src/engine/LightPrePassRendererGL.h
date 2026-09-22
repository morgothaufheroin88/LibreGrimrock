// Light pre-pass renderer, reconstructed from LightPrePassRendererGL.cpp
// (0x0811a320-0x08122030). Object layout follows the original (0x80 bytes).
#pragma once
#include "core/Array.h"
#include "engine/RenderContextGL.h"
#include "engine/RendererGL.h"

namespace engine
{

class Camera;
class LightEntity;
class MeshEntity;
class RenderVisitor;
class ParticleSystemRendererGL;

class LightPrePassRendererGL
{
  public:
    enum RenderPass
    {
        GeometryPass = 0,
        MaterialPass = 1,
        TransparentPass = 2
    };

    LightPrePassRendererGL(RenderContextGL* context, int width, int height);
    ~LightPrePassRendererGL();

    void setViewport(int x, int y, int width, int height);
    void resizeRenderBuffers(int width, int height);
    void renderGeometryPass(const Camera& camera, const RenderVisitor& visitor);
    void renderLightPass(const Camera& camera, const RenderVisitor& visitor);
    void renderMaterialPass(const Camera& camera, const RenderVisitor& visitor);
    void renderTransparentPass(const Camera& camera, const RenderVisitor& visitor);
    // 0x0811f1b0: shadow map of the static geometry around a light.
    TextureCubeGL* renderStaticShadowMap(const LightEntity& light, int size);

    Texture2DGL* getGeometryBuffer() const
    {
        return m_pGeometryBuffer;
    }
    Texture2DGL* getGlossinessBuffer() const
    {
        return m_pGlossinessBuffer;
    }
    Texture2DGL* getLightBuffer() const
    {
        return m_pLightBuffer;
    }
    Texture2DGL* getColorBuffer() const
    {
        return m_pColorBuffer;
    }
    GLuint getDepthStencilBuffer() const
    {
        return m_depthStencilBuffer;
    }

    // settings copied from the renderer each frame (+0x1c..+0x24)
    bool m_diffuseMapping;
    bool m_normalMapping;
    bool m_renderMeshes;
    bool m_renderShadows;
    int m_textureFilter;
    int m_shadowQuality;

  private:
    void renderMesh(const Camera& camera, const MeshEntity& entity, RenderPass pass);
    void renderAmbientLight(const Camera& camera, const LightEntity& light);
    void renderDirectionalLight(const Camera& camera, const LightEntity& light);
    void renderPointLight(const Camera& camera, const LightEntity& light, TextureCubeGL* shadowMap);
    void renderPointLightStencil(const Camera& camera, const LightEntity& light);
    void renderSpotLight(const Camera& camera, const LightEntity& light);
    // 0x0811a8c0: returns the covered pixel count, 0 if the light is not visible.
    int setupPointLightScissorRect(const Camera& camera, const core::Vec3& lightPos, float radius);
    void renderPointLightShadowMap(const LightEntity& light, TextureCubeGL* cube,
                                   GLuint depthBuffer, bool staticOnly);
    void blurCubeMapGPU(TextureCubeGL* source, TextureCubeGL* target, float blurAngle);

    RenderContextGL* m_pContext;
    int m_width;
    int m_height;
    int m_viewportX, m_viewportY, m_viewportWidth, m_viewportHeight;
    ParticleSystemRendererGL* m_pParticleRenderer;
    GLuint m_depthStencilBuffer;
    Texture2DGL* m_pGeometryBuffer;
    Texture2DGL* m_pGlossinessBuffer;
    Texture2DGL* m_pLightBuffer;
    Texture2DGL* m_pColorBuffer;
    // indexed by log2(size): 1..256, entries below 16 are null
    core::Array<TextureCubeGL*> m_shadowMaps;
    core::Array<TextureCubeGL*> m_blurTemps;
    core::Array<GLuint> m_shadowDepthBuffers;
    RenderableShaderGL* m_pDefaultShader;
    ShaderProgramGL* m_pAmbientLightProgram;
    ShaderProgramGL* m_pPointLightProgram;
    ShaderProgramGL* m_pPointLightShadowProgram;
    ShaderProgramGL* m_pDirectionalLightProgram;
    ShaderProgramGL* m_pStencilProgram;
    ShaderProgramGL* m_pBlurCubeMapProgram;
};

} // namespace engine
