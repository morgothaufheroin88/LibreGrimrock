// Reconstructed from grimrock2.exe ParticleSystemRendererGL.cpp.
#include "engine/ParticleSystemRendererGL.h"
#include "core/Math.h"
#include "engine/Camera.h"
#include "engine/ParticleSystem.h"
#include "engine/RendererGL.h"
#include <cfloat>
#include <cmath>

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

// 0x004f6e70: the HEIGHTMAP variant clamps the particles to a height map texture.
ParticleSystemRendererGL::ParticleSystemRendererGL(RenderContextGL* context)
    : m_pContext(context), m_vertexArray(0), m_pProgram(0), m_pHeightmapProgram(0)
{
    static constexpr const char* defines[] = {"HEIGHTMAP", 0};
    GLuint vs = RenderContextGL::compileShaderFromFile("shaders/gl/ParticleSystem.vsh",
                                                       GL_VERTEX_SHADER, 0);
    GLuint vsHeightmap = RenderContextGL::compileShaderFromFile("shaders/gl/ParticleSystem.vsh",
                                                                GL_VERTEX_SHADER, defines);
    GLuint fs = RenderContextGL::compileShaderFromFile("shaders/gl/ParticleSystem.fsh",
                                                       GL_FRAGMENT_SHADER, 0);
    m_pProgram = new ShaderProgramGL(vs, fs);
    m_pHeightmapProgram = new ShaderProgramGL(vsHeightmap, fs);
    glDeleteShader(vs);
    glDeleteShader(vsHeightmap);
    glDeleteShader(fs);
    glGenVertexArrays(1, &m_vertexArray);
    glBindVertexArray(m_vertexArray);
    glEnableVertexAttribArray(ShaderProgramGL::A_position);
    glEnableVertexAttribArray(ShaderProgramGL::A_velocity);
    glEnableVertexAttribArray(ShaderProgramGL::A_texcoord);
    glEnableVertexAttribArray(ShaderProgramGL::A_particleParms);
    glBindVertexArray(0);
}
// 0x004f7030
ParticleSystemRendererGL::~ParticleSystemRendererGL()
{
    delete m_pHeightmapProgram;
    delete m_pProgram;
    glDeleteVertexArrays(1, &m_vertexArray);
}

// 0x004f7080
void ParticleSystemRendererGL::renderParticleSystem(const Camera& camera,
                                                    const ParticleEntity& entity,
                                                    bool diffuseMapping,
                                                    const Matrix4x4& projection)
{
    const Array<ParticleState*>& states = entity.getStates();
    for (int s = 0; s < states.size(); ++s)
    {
        ParticleState* state = states[s];
        const ParticleEmitter* emitter = state->getEmitter();
        if (state->getNumAlive() == 0)
            continue;
        // the entity fades out between the fade distances from the camera
        float opacity = entity.getOpacity() * emitter->m_opacity;
        if (entity.getDistanceFadeStart() != 0.0f || entity.getDistanceFadeEnd() != 0.0f)
        {
            float start = entity.getDistanceFadeStart(), end = entity.getDistanceFadeEnd();
            Vec3 d =
                entity.getNode()->getLocalToWorldMatrix().pos - camera.getLocalToWorldMatrix().pos;
            float fade = 1.0f - (d.length() - start) / (end - start);
            if (fade >= 1.0f)
                fade = 1.0f;
            opacity *= fade;
        }
        if (opacity <= 0.0f)
            continue;
        if (emitter->m_blendMode == ParticleEmitter::AdditiveBlend)
            m_pContext->setBlendMode(RenderContextGL::Blend_AdditiveSrcAlpha);
        else
            m_pContext->setBlendMode(RenderContextGL::Blend_Translucent);
        glDisable(GL_CULL_FACE);
        RenderableTexture* texture = emitter->m_texture.get();
        RenderableTexture* heightmap = 0;
        ShaderProgramGL* program = m_pProgram;
        if (emitter->m_clampToGroundPlane && entity.getHeightmap())
        {
            heightmap = entity.getHeightmap();
            program = m_pHeightmapProgram;
        }
        m_pContext->useProgram(program);
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
        int framesPerRow = texture->getWidth() / frameSize;
        Matrix4x4 localToWorld;
        if (emitter->m_objectSpace)
            localToWorld = Matrix4x4(entity.getNode()->getLocalToWorldMatrix());
        program->setUniform(ShaderProgramGL::U_localToWorld, localToWorld);
        Matrix4x4 view(camera.getWorldToLocalMatrix());
        program->setUniform(ShaderProgramGL::U_modelView, view);
        Matrix4x4 proj =
            RenderContextGL::sm_d3dToGLProj *
            (camera.getUserClipPlaneMask() == 1 ? projection : camera.getProjectionMatrix());
        program->setUniform(ShaderProgramGL::U_proj, proj);
        program->setUniform(
            ShaderProgramGL::U_fadeParms,
            Vec3(1.0f / emitter->m_fadeIn, 1.0f / emitter->m_fadeOut, emitter->m_fadeOut));
        program->setUniform(ShaderProgramGL::U_textureAnimParms,
                            Vec4(frameRate, (float)frameCount, (float)framesPerRow,
                                 (float)frameSize / (float)texture->getWidth()));
        program->setUniform(ShaderProgramGL::U_gravity, emitter->m_gravity);
        program->setUniform(ShaderProgramGL::U_airResistance, emitter->m_airResistance);
        program->setUniform(
            ShaderProgramGL::U_rotationParams,
            Vec2(emitter->m_rotationSpeed, emitter->m_randomInitialRotation ? TWO_PI : 0.0f));
        program->setUniform(ShaderProgramGL::U_clampToGroundPlane,
                            emitter->m_clampToGroundPlane ? 0.0f : -FLT_MAX);
        program->setUniform(ShaderProgramGL::U_depthBias, emitter->m_depthBias);
        Vec3 colors[4];
        for (int i = 0; i < 4; ++i)
            colors[i] = emitter->m_colorAnimation ? emitter->m_color[i] : emitter->m_color[0];
        program->setUniform3v(ShaderProgramGL::U_colorTable, &colors[0].x, 4);
        program->setUniform(ShaderProgramGL::U_opacity, opacity);
        RenderableTextureGL* tex =
            (RenderableTextureGL*)(diffuseMapping ? texture : CommonResourcesGL::WhiteMap);
        m_pContext->setUniformTexture(ShaderProgramGL::U_texture, tex->getTexture(), -1,
                                      Material::Clamp, 0);
        if (heightmap)
            m_pContext->setUniformTexture("g_heightmap",
                                          ((RenderableTextureGL*)heightmap)->getTexture(),
                                          Material::Linear, Material::Clamp, 1);

        glBindVertexArray(m_vertexArray);
        int numQuads = state->getNumAlive();
        int bytes = numQuads * 4 * (int)sizeof(ParticleVertex);
        ParticleVertex* out = (ParticleVertex*)m_pContext->streamWrite(bytes);
        if (!out)
            continue;
        const char* base = (const char*)(intptr_t)(m_pContext->getStreamOffset() - bytes);
        glVertexAttribPointer(ShaderProgramGL::A_position, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), base);
        glVertexAttribPointer(ShaderProgramGL::A_velocity, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), base + 12);
        glVertexAttribPointer(ShaderProgramGL::A_texcoord, 2, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), base + 24);
        glVertexAttribPointer(ShaderProgramGL::A_particleParms, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), base + 32);
        const Array<Particle>& particles = state->getParticles();
        static constexpr float corners[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        int written = 0;
        for (int i = 0; i < particles.size() && written < numQuads; ++i)
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
            ++written;
        }
        m_pContext->streamDrawPrimitive(RenderContextGL::Prim_Quads, numQuads * 4);
        g_renderStats.renderTriangles += numQuads * 2;
    }
}

} // namespace engine
