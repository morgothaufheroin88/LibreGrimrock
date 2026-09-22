// Light pre-pass renderer of Legend of Grimrock 2, reconstructed from grimrock2.exe
// LightPrePassRendererGL.cpp (0x004e5ba0-0x004ed800). Object layout follows the original
// (0x218 bytes): draw calls are sorted by material and program, point and spot lights
// carry blurred shadow maps, directional lights cascaded ones, and meshes can dissolve.
#pragma once
#include "core/Array.h"
#include "core/Color.h"
#include "engine/RenderContextGL.h"
#include "engine/RendererGL.h"

namespace engine
{

class Camera;
class LightEntity;
class MeshEntity;
class RenderVisitor;
class ParticleSystemRendererGL;
class BlurGL;

class LightPrePassRendererGL
{
  public:
    // renderMaterialPass/renderMesh pass selectors
    enum MaterialPassType
    {
        MaterialPass_AmbientOcclusion = 1, // materials shaded with the occlusion buffer
        MaterialPass_NoAmbientOcclusion = 2,
        MaterialPass_Transparent = 3
    };
    // renderTransparentPass pass numbers, selected by the entity render hack bits
    enum TransparentPass
    {
        TransparentPass_First = 4,
        TransparentPass_Second = 5,
        TransparentPass_Last = 6
    };
    // RenderEntity render hack bits used by the transparent pass
    enum RenderHack
    {
        RenderHack_DepthWrite = 1,
        RenderHack_FirstPass = 2,
        RenderHack_SecondPass = 4,
        RenderHack_NoDepthTest = 8
    };
    static constexpr int NumShadowMapSizes = 16; // indexed by log2(size)
    static constexpr int NumCascades = 4;
    static constexpr int StaticShadowMapSizeIndexLimit = 12;
    // six frustum planes and one per silhouette edge of a cascade (0x004ecad0 reserves 16)
    static constexpr int MaxShadowCasterPlanes = 16;

    // 0x004e90e0
    LightPrePassRendererGL(RenderContextGL* context, int width, int height);
    // 0x004e9be0
    ~LightPrePassRendererGL();

    // 0x004e5c70
    void setViewport(int x, int y, int width, int height);
    // 0x004e7d90
    void resizeRenderBuffers(int width, int height);
    // 0x004ea140
    void renderGeometryPass(const Camera& camera, const RenderVisitor& visitor);
    // 0x004ed100
    void renderLightPass(const Camera& camera, const RenderVisitor& visitor);
    // 0x004eb990
    void renderMaterialPass(const Camera& camera, const RenderVisitor& visitor, int pass);
    // 0x004ebad0
    void renderTransparentPass(const Camera& camera, const RenderVisitor& visitor, int pass);
    // 0x004ed600: shadow map of the static geometry around a light (cube for point lights,
    // 2D for spot lights); extents receives the reach of the geometry per cube face.
    TextureGL* renderStaticShadowMap(LightEntity& light, int size, float* extents);

    RenderableTextureGL* getGeometryBuffer() const
    {
        return m_pGeometryBuffer.get();
    }
    Texture2DGL* getGlossinessBuffer() const
    {
        return m_pGlossinessBuffer;
    }
    Texture2DGL* getLightBuffer() const
    {
        return m_pLightBuffer;
    }
    Texture2DGL* getFrameBuffer() const
    {
        return m_pFrameBuffer;
    }
    GLuint getDepthStencilBuffer() const
    {
        return m_depthStencilBuffer;
    }
    // Projection of the current camera with the oblique near plane applied when the first
    // user clip plane is active (0x1d8).
    const core::Matrix4x4& getProjection() const
    {
        return m_projection;
    }

  private:
    // 0x004ea9d0: sorted draw of the given meshes for a material pass
    void renderMeshes(const Camera& camera, MeshEntity* const* meshes, int count, int pass);
    // 0x004eb2a0: shadow casters with the given view/projection and shader variant
    void renderShadowMeshes(const core::Array<MeshEntity*>& meshes, const core::Matrix4x4& view,
                            const core::Matrix4x4& projection, float invLightRange, bool staticOnly,
                            int variant);
    // 0x004e5ca0
    void renderAmbientLight(const Camera& camera, const LightEntity& light);
    // 0x004e6990: one cascade (shadowMap null = unshadowed)
    void renderDirectionalLight(const Camera& camera, const LightEntity& light,
                                Texture2DGL* shadowMap, const core::Matrix4x4& shadowViewProj,
                                int cascade, float cascadeStart, float cascadeEnd);
    // 0x004e7fb0
    void renderPointLight(const Camera& camera, const LightEntity& light, TextureCubeGL* shadowMap);
    // 0x004e6320
    void renderPointLightStencil(const Camera& camera, const LightEntity& light);
    // 0x004e8440
    void renderSpotLight(const Camera& camera, const LightEntity& light, Texture2DGL* shadowMap);
    // 0x004e6520
    void renderSpotLightStencil(const Camera& camera, const LightEntity& light);
    // 0x004e72b0: returns the covered pixel count, 0 if the light is not visible.
    int setupPointLightScissorRect(const Camera& camera, const core::Vec3& lightPos, float range);
    // 0x004ebe70: the faces in faceMask of the cube around a point light
    void renderPointLightShadowMap(const LightEntity& light, const Camera& camera,
                                   TextureCubeGL* cube, GLuint depthBuffer, bool staticOnly,
                                   unsigned int faceMask);
    // 0x004ec740
    void renderSpotLightShadowMap(const LightEntity& light, const Camera& camera,
                                  Texture2DGL* shadowMap, GLuint depthBuffer, bool staticOnly);
    // 0x004ecad0: orthographic map of one cascade, returns the light view projection
    void renderDirectionalLightShadowMap(const LightEntity& light, const Camera& camera,
                                         Texture2DGL* shadowMap, GLuint depthBuffer,
                                         float cascadeStart, float cascadeEnd,
                                         core::Matrix4x4& shadowViewProj);
    // 0x004e5e00: 16 jittered samples in a cone of blurAngle around each texel direction
    void blurCubeMapGPU(TextureCubeGL* source, TextureCubeGL* target, float blurAngle,
                        unsigned int faceMask);
    // 0x004e7740 / 0x004e7820 / 0x004e6720: allocated on first use per size
    void getShadowMap(int size, Texture2DGL*& map, Texture2DGL*& temp);
    void getShadowCubeMap(int size, TextureCubeGL*& map, TextureCubeGL*& temp);
    GLuint getShadowDepthBuffer(int size);
    static int sizeIndex(int size);
    void setViewportGL();

    RenderContextGL* m_pContext;
    int m_width;
    int m_height;
    int m_viewportX, m_viewportY, m_viewportWidth, m_viewportHeight;

  public:
    // settings the renderer copies in before the passes (+0x1c..+0x28)
    core::Color m_clearColor;
    bool m_diffuseMapping;
    bool m_normalMapping;
    bool m_renderMeshes;
    bool m_renderShadows;
    int m_textureFilter;
    int m_shadowQuality;

  private:
    ParticleSystemRendererGL* m_pParticleRenderer;
    RenderVisitor* m_pShadowVisitor;
    BlurGL* m_pBlur;
    GLuint m_depthStencilBuffer;
    // shared: the scripts hand it to materials, which reference count it
    core::SharedPtr<RenderableTextureGL> m_pGeometryBuffer;
    Texture2DGL* m_pNormalBuffer;
    Texture2DGL* m_pGlossinessBuffer;
    Texture2DGL* m_pLightBuffer;
    Texture2DGL* m_pFrameBuffer;
    Texture2DGL* m_shadowMaps[NumShadowMapSizes];
    Texture2DGL* m_shadowMapTemps[NumShadowMapSizes];
    TextureCubeGL* m_shadowCubeMaps[NumShadowMapSizes];
    TextureCubeGL* m_shadowCubeMapTemps[NumShadowMapSizes];
    GLuint m_shadowDepthBuffers[NumShadowMapSizes];
    RenderableShaderGL* m_pDefaultShader;
    RenderableShaderGL* m_pDissolveShader;
    ShaderProgramGL* m_pAmbientLightProgram;
    // [shadow | specular << 1]
    ShaderProgramGL* m_pointLightPrograms[4];
    ShaderProgramGL* m_spotLightPrograms[4];
    ShaderProgramGL* m_directionalLightPrograms[4];
    ShaderProgramGL* m_pStencilProgram;
    ShaderProgramGL* m_pSpotLightStencilProgram;
    ShaderProgramGL* m_pBlurCubeMapProgram;
    core::Matrix4x4 m_projection;
};

} // namespace engine
