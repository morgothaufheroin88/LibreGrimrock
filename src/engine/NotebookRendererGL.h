// Forward renderer for low end GPUs, reconstructed from NotebookRendererGL.cpp
// (0x08126c30-0x08129000).
#pragma once
#include "core/Vector.h"
#include "engine/RenderContextGL.h"

namespace engine
{

class Camera;
class MeshEntity;
class LightEntity;
class RenderVisitor;
class ParticleSystemRendererGL;

// [0] plain, [1] skinning, [2] normal map, [3] normal map + skinning, [4] unlit,
// [5] unlit + skinning.
class NotebookRendererShaderGL
{
  public:
    enum
    {
        Skinning = 1,
        NormalMap = 2,
        Unlit = 4
    };
    NotebookRendererShaderGL();
    ~NotebookRendererShaderGL();
    ShaderProgramGL* getProgram(int variant) const
    {
        return m_programs[variant];
    }

  private:
    ShaderProgramGL* m_programs[6];
};

class NotebookRendererGL
{
  public:
    enum RenderPass
    {
        OpaquePass = 0,
        TransparentPass = 1
    };
    static constexpr int MaxVertexLights = 6;
    static constexpr int MaxShaderVertexLights = 4;
    NotebookRendererGL(RenderContextGL* context);
    virtual ~NotebookRendererGL();
    void renderOpaquePass(const Camera& camera, const RenderVisitor& visitor);
    void renderTransparentPass(const Camera& camera, const RenderVisitor& visitor);

    bool m_diffuseMapping;
    bool m_normalMapping;

  private:
    void renderMesh(const Camera& camera, const MeshEntity& entity, const LightEntity* primaryLight,
                    RenderPass pass);

    RenderContextGL* m_pContext;
    ParticleSystemRendererGL* m_pParticleRenderer;
    NotebookRendererShaderGL* m_pShader;
    core::Vec3 m_vlightColor[MaxVertexLights];
    core::Vec3 m_vlightPosition[MaxVertexLights];
    float m_vlightInvRange[MaxVertexLights];
};

} // namespace engine
