// Particle quad renderer of Legend of Grimrock 2, reconstructed from grimrock2.exe
// ParticleSystemRendererGL.cpp (0x004f6e70-0x004f7700).
#pragma once
#include "core/Matrix.h"
#include "engine/RenderContextGL.h"

namespace engine
{

class Camera;
class ParticleEntity;

class ParticleSystemRendererGL
{
  public:
    // 0x004f6e70
    ParticleSystemRendererGL(RenderContextGL* context);
    ~ParticleSystemRendererGL();
    // 0x004f7080: one quad per live particle, expanded in the vertex shader; projection
    // is the renderer's (possibly oblique) projection of the camera.
    void renderParticleSystem(const Camera& camera, const ParticleEntity& entity,
                              bool diffuseMapping, const core::Matrix4x4& projection);

  private:
    RenderContextGL* m_pContext;
    GLuint m_vertexArray;
    ShaderProgramGL* m_pProgram;
    ShaderProgramGL* m_pHeightmapProgram;
};

} // namespace engine
