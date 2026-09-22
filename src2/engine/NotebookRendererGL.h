// Forward renderer for low end GPUs of Legend of Grimrock 2, reconstructed from
// grimrock2.exe NotebookRendererGL.cpp (0x004edb60-0x004ef2a0): one primary light per
// pixel, up to 16 vertex lights, an ambient and a directional light and linear fog,
// with the draw calls sorted by program and material like the light pre-pass renderer.
#pragma once
#include "core/Array.h"
#include "core/Vector.h"
#include "engine/RenderContextGL.h"

namespace engine
{

class Camera;
class MeshEntity;
class LightEntity;
class RenderVisitor;
class RenderableShaderGL;
class ParticleSystemRendererGL;

// 0x274 bytes
class NotebookRendererGL
{
  public:
    enum RenderPass
    {
        OpaquePass = 0,
        TransparentPass1 = 1,
        TransparentPass2 = 2
    };
    static constexpr int MaxVertexLights = 16;
    // 0x004ede30
    NotebookRendererGL(RenderContextGL* context);
    // 0x004edfd0
    virtual ~NotebookRendererGL();
    // 0x004edb60: only a depth buffer is needed, the colour goes to the target
    void resizeRenderBuffers(int width, int height);
    // 0x004ee9d0: gathers the lights, then draws the opaque meshes
    void renderOpaquePass(const Camera& camera, const RenderVisitor& visitor);
    // 0x004ef030: blended meshes of the pass back to front, particles in pass 1
    void renderTransparentPass(const Camera& camera, const RenderVisitor& visitor, int pass);
    GLuint getDepthBuffer() const
    {
        return m_depthBuffer;
    }

    bool m_diffuseMapping;
    bool m_normalMapping;
    core::Vec3 m_fogColor;
    float m_fogStart;
    float m_fogEnd;
    int m_width;
    int m_height;

  private:
    // 0x004ee0d0: sorted draw of the meshes (pass 0 opaque, 1 blended)
    void renderMeshes(const Camera& camera, MeshEntity* const* meshes, int count, int pass);

    RenderContextGL* m_pContext;
    GLuint m_depthBuffer;
    ParticleSystemRendererGL* m_pParticleRenderer;
    RenderableShaderGL* m_pShader;
    core::Vec3 m_ambientLight;
    core::Vec3 m_dirlightColor;
    core::Vec3 m_dirlightDirection;
    core::Vec3 m_lightColor;
    core::Vec3 m_lightPosition;
    float m_invLightRange;
    core::Vec3 m_vlightColor[MaxVertexLights];
    core::Vec4 m_vlightPosition[MaxVertexLights]; // xyz view space, w = 1 / half range
    core::Matrix4x4 m_projection;                 // oblique when the first clip plane is set
};

} // namespace engine
