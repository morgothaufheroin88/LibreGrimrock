// Particle quad renderer, reconstructed from ParticleSystemRendererGL.cpp (0x08122f40-0x08123a00).
#pragma once
#include "engine/RenderContextGL.h"

namespace engine
{

class Camera;
class ParticleEntity;

class ParticleSystemRendererGL
{
  public:
    ParticleSystemRendererGL(RenderContextGL* context);
    ~ParticleSystemRendererGL();
    // One quad per live particle, expanded in the vertex shader.
    void renderParticleSystem(const Camera& camera, const ParticleEntity& entity,
                              bool diffuseMapping);

  private:
    RenderContextGL* m_pContext;
    ShaderProgramGL* m_pProgram;
};

} // namespace engine
