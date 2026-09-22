// Reconstructed from Grimrock.bin.x86 ParticleSystemRendererGL.cpp.
#include "engine/ParticleSystemRendererGL.h"
#include "engine/Camera.h"
#include "engine/ParticleSystem.h"
#include "engine/RendererGL.h"
#include <cfloat>

namespace engine
{

using namespace core;

// 48 bytes: position, velocity, texcoord, particle parameters (time, size, random, lifetime).
struct ParticleVertex
{
    Vec3 pos;
    Vec3 velocity;
    float u, v;
    float time, size, random, lifetime;
};

// 0x08122f40
ParticleSystemRendererGL::ParticleSystemRendererGL(RenderContextGL* context)
    : m_pContext(context), m_pProgram(0)
{
    m_pProgram =
        new ShaderProgramGL("shaders/gl/ParticleSystem.vsh", "shaders/gl/ParticleSystem.fsh");
}
ParticleSystemRendererGL::~ParticleSystemRendererGL()
{
    delete m_pProgram;
}

// 0x08122fd0
void ParticleSystemRendererGL::renderParticleSystem(const Camera& camera,
                                                    const ParticleEntity& entity,
                                                    bool diffuseMapping)
{
    const Array<ParticleState*>& states = entity.getStates();
    for (int s = 0; s < states.size(); ++s)
    {
        ParticleState* state = states[s];
        const ParticleEmitter* emitter = state->getEmitter();
        if (state->getNumAlive() == 0)
            continue;
        m_pContext->useProgram(m_pProgram);
        if (emitter->m_blendMode == ParticleEmitter::AdditiveBlend)
            m_pContext->setBlendMode(RenderContextGL::Blend_Premultiplied);
        else
            m_pContext->setBlendMode(RenderContextGL::Blend_Translucent);
        glDisable(GL_CULL_FACE);
        RenderableTexture* texture = emitter->m_texture.get();
        float frameRate = emitter->m_frameRate;
        int frameSize = emitter->m_frameSize;
        if (frameSize == 0)
            frameSize = texture->getWidth();
        int frameCount = emitter->m_frameCount;
        if (frameCount == 0)
        {
            frameCount = 1;
            frameRate = 0.0f;
        }
        int textureWidth = texture->getWidth();
        Matrix4x4 localToWorld;
        if (emitter->m_objectSpace)
            localToWorld = Matrix4x4(entity.getNode()->getLocalToWorldMatrix());
        m_pProgram->setUniform(ShaderProgramGL::U_localToWorld, localToWorld);
        Matrix4x4 view(camera.getWorldToLocalMatrix());
        m_pProgram->setUniform(ShaderProgramGL::U_modelView, view);
        Matrix4x4 proj = RenderContextGL::sm_d3dToGLProj * camera.getProjectionMatrix();
        m_pProgram->setUniform(ShaderProgramGL::U_proj, proj);
        m_pProgram->setUniform(
            ShaderProgramGL::U_fadeParms,
            Vec3(1.0f / emitter->m_fadeIn, 1.0f / emitter->m_fadeOut, emitter->m_fadeOut));
        m_pProgram->setUniform(ShaderProgramGL::U_textureAnimParms,
                               Vec4(frameRate, (float)frameCount, (float)(textureWidth / frameSize),
                                    (float)frameSize / (float)textureWidth));
        m_pProgram->setUniform(ShaderProgramGL::U_gravity, emitter->m_gravity);
        m_pProgram->setUniform(ShaderProgramGL::U_airResistance, emitter->m_airResistance);
        m_pProgram->setUniform(ShaderProgramGL::U_rotationSpeed, emitter->m_rotationSpeed);
        m_pProgram->setUniform(ShaderProgramGL::U_clampToGroundPlane,
                               emitter->m_clampToGroundPlane ? 0.0f : -FLT_MAX);
        m_pProgram->setUniform(ShaderProgramGL::U_depthBias, emitter->m_depthBias);
        Vec3 colors[4];
        for (int i = 0; i < 4; ++i)
            colors[i] = emitter->m_colorAnimation ? emitter->m_color[i] : emitter->m_color[0];
        m_pProgram->setUniform3v(ShaderProgramGL::U_colorTable, &colors[0].x, 4);
        m_pProgram->setUniform(ShaderProgramGL::U_opacity, emitter->m_opacity);
        RenderableTextureGL* tex =
            (RenderableTextureGL*)(diffuseMapping ? texture : CommonResourcesGL::WhiteMap);
        m_pContext->setUniformTexture(ShaderProgramGL::U_texture, tex->getTexture(), -1, -1);

        ParticleVertex* out = (ParticleVertex*)m_pContext->streamWrite(state->getNumAlive() * 4 *
                                                                       sizeof(ParticleVertex));
        if (!out)
            continue;
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), &out->pos);
        glEnableVertexAttribArray(8);
        glVertexAttribPointer(8, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), &out->velocity);
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), &out->u);
        glEnableVertexAttribArray(9);
        glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), &out->time);
        const Array<Particle>& particles = state->getParticles();
        int numQuads = 0;
        static constexpr float corners[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        for (int i = 0; i < particles.size(); ++i)
        {
            const Particle& p = particles[i];
            if (p.time < 0.0f)
                continue;
            for (int c = 0; c < 4; ++c)
            {
                out->pos = p.pos;
                out->velocity = p.velocity;
                out->u = corners[c][0];
                out->v = corners[c][1];
                out->time = p.time;
                out->size = p.size;
                out->random = p.random;
                out->lifetime = p.lifetime;
                ++out;
            }
            ++numQuads;
        }
        m_pContext->streamDrawPrimitive(RenderContextGL::Prim_Quads, numQuads);
    }
    for (int i = 0; i < 10; ++i)
        glDisableVertexAttribArray(i);
}

} // namespace engine
