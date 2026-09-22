// Reconstructed from Grimrock.bin.x86 NotebookRendererGL.cpp.
#include "engine/NotebookRendererGL.h"
#include "engine/Camera.h"
#include "engine/ParticleSystem.h"
#include "engine/ParticleSystemRendererGL.h"
#include "engine/RenderEntity.h"
#include "engine/Renderer.h"
#include "engine/RendererGL.h"
#include <algorithm>
#include <cstring>

namespace engine
{

using namespace core;

namespace
{
struct SortItem
{
    RenderEntity* entity;
    float depth;
    bool operator<(const SortItem& o) const
    {
        return depth > o.depth;
    }
};
} // namespace

static const Material* segmentMaterial(const MeshEntity& entity, int segment)
{
    if (segment < entity.getMaterials().size())
        return entity.getMaterials()[segment].get();
    return Material::Default.get();
}

// 0x08126c30
NotebookRendererShaderGL::NotebookRendererShaderGL()
{
    memset(m_programs, 0, sizeof(m_programs));
    static constexpr const char* defSkin[] = {"SKINNING", 0};
    static constexpr const char* defNormal[] = {"NORMAL_MAP", 0};
    static constexpr const char* defNormalSkin[] = {"NORMAL_MAP", "SKINNING", 0};
    GLuint vs = RenderContextGL::compileShaderFromFile("shaders/gl/NotebookRenderer.vsh",
                                                       GL_VERTEX_SHADER, 0);
    GLuint vsSkin = RenderContextGL::compileShaderFromFile("shaders/gl/NotebookRenderer.vsh",
                                                           GL_VERTEX_SHADER, defSkin);
    GLuint vsNormal = RenderContextGL::compileShaderFromFile("shaders/gl/NotebookRenderer.vsh",
                                                             GL_VERTEX_SHADER, defNormal);
    GLuint vsNormalSkin = RenderContextGL::compileShaderFromFile("shaders/gl/NotebookRenderer.vsh",
                                                                 GL_VERTEX_SHADER, defNormalSkin);
    GLuint fs = RenderContextGL::compileShaderFromFile("shaders/gl/NotebookRenderer.fsh",
                                                       GL_FRAGMENT_SHADER, 0);
    GLuint fsNormal = RenderContextGL::compileShaderFromFile("shaders/gl/NotebookRenderer.fsh",
                                                             GL_FRAGMENT_SHADER, defNormal);
    GLuint fsUnlit = RenderContextGL::compileShaderFromFile("shaders/gl/NotebookRendererUnlit.fsh",
                                                            GL_FRAGMENT_SHADER, 0);
    m_programs[0] = new ShaderProgramGL(vs, fs);
    m_programs[1] = new ShaderProgramGL(vsSkin, fs);
    m_programs[2] = new ShaderProgramGL(vsNormal, fsNormal);
    m_programs[3] = new ShaderProgramGL(vsNormalSkin, fsNormal);
    m_programs[4] = new ShaderProgramGL(vs, fsUnlit);
    m_programs[5] = new ShaderProgramGL(vsSkin, fsUnlit);
}
NotebookRendererShaderGL::~NotebookRendererShaderGL()
{
    for (int i = 5; i >= 0; --i)
        delete m_programs[i];
}

// 0x08126f00
NotebookRendererGL::NotebookRendererGL(RenderContextGL* context)
    : m_diffuseMapping(true), m_normalMapping(true), m_pContext(context), m_pParticleRenderer(0),
      m_pShader(0)
{
    for (int i = 0; i < MaxVertexLights; ++i)
    {
        m_vlightColor[i].set(0, 0, 0);
        m_vlightPosition[i].set(0, 0, 0);
        m_vlightInvRange[i] = 0.0f;
    }
    m_pParticleRenderer = new ParticleSystemRendererGL(m_pContext);
    m_pShader = new NotebookRendererShaderGL;
}
// 0x08127170
NotebookRendererGL::~NotebookRendererGL()
{
    delete m_pShader;
    delete m_pParticleRenderer;
}

// 0x08127240: one per pixel light plus vertex lights.
void NotebookRendererGL::renderMesh(const Camera& camera, const MeshEntity& entity,
                                    const LightEntity* primaryLight, RenderPass pass)
{
    RenderableMeshGL* mesh = (RenderableMeshGL*)entity.getMesh();
    bool skinned = mesh->isSkinned() && entity.getSkeleton() != 0;
    bool normalMapped = false;
    if (mesh->hasTangents() && m_normalMapping)
    {
        for (int i = 0; i < mesh->getNumSegments(); ++i)
        {
            if (segmentMaterial(entity, i)->getNormalMap())
            {
                normalMapped = true;
                break;
            }
        }
    }
    mesh->activate();
    glAlphaFunc(GL_GREATER, 0.5f);
    for (int i = 0; i < mesh->getNumSegments(); ++i)
    {
        const Material* material = segmentMaterial(entity, i);
        bool transparent = pass == TransparentPass;
        if ((material->getBlendMode() == Material::Opaque) == transparent)
            continue;
        int variant = skinned ? NotebookRendererShaderGL::Skinning : 0;
        if (!material->getLighting() || transparent)
            variant |= NotebookRendererShaderGL::Unlit;
        else if (normalMapped)
            variant |= NotebookRendererShaderGL::NormalMap;
        m_pContext->useProgram(m_pShader->getProgram(variant));
        ShaderProgramGL* program = m_pContext->getProgram();
        GLuint prog = program->getProgram();

        const Matrix4x3& localToWorld = entity.getNode()->getLocalToWorldMatrix();
        Matrix4x4 mvp = RenderContextGL::sm_d3dToGLProj *
                        (camera.getViewProjectionMatrix() * Matrix4x4(localToWorld));
        program->setUniform(ShaderProgramGL::U_modelViewProj, mvp);
        Matrix4x4 modelView(camera.getWorldToLocalMatrix() * localToWorld);
        program->setUniform(ShaderProgramGL::U_modelView, modelView);
        if (skinned)
            m_pContext->setSkinningMatrices(entity);
        const Vec3& emissive = entity.getEmissiveColor();
        program->setUniform("emissiveColor", emissive);
        program->setUniform(ShaderProgramGL::U_glossiness, material->getGlossiness());
        program->setUniform3v("vlightColor", &m_vlightColor[0].x, MaxShaderVertexLights);
        program->setUniform3v("vlightPosition", &m_vlightPosition[0].x, MaxShaderVertexLights);
        glUniform1fv(glGetUniformLocation(prog, "vlightInvRange"), MaxShaderVertexLights,
                     m_vlightInvRange);
        if (!primaryLight)
        {
            program->setUniform("lightColor", Vec3(0, 0, 0));
            program->setUniform("lightPosition", Vec3(0, 0, 0));
            program->setUniform("invLightRange", 1.0f);
        }
        else
        {
            const Vec3& lightColor = primaryLight->getLightColor();
            program->setUniform("lightColor", lightColor);
            Vec3 lightPos = camera.getWorldToLocalMatrix().transformPoint(
                primaryLight->getNode()->getLocalToWorldMatrix().pos);
            program->setUniform("lightPosition", lightPos);
            program->setUniform("invLightRange", 1.0f / primaryLight->getLightRange());
        }
        if (material->getDoubleSided())
            glDisable(GL_CULL_FACE);
        else
            glEnable(GL_CULL_FACE);
        m_pContext->setBlendMode(material->getBlendMode());
        if (material->getAlphaTest())
            glEnable(GL_ALPHA_TEST);
        else
            glDisable(GL_ALPHA_TEST);
        int address = material->getTextureAddressMode();
        int filter = Material::Linear_MipNearest;
        RenderableTexture* diffuse = material->getDiffuseMap();
        if (!diffuse || !m_diffuseMapping)
            diffuse = CommonResourcesGL::WhiteMap;
        m_pContext->setUniformTexture(ShaderProgramGL::U_diffuseMap,
                                      ((RenderableTextureGL*)diffuse)->getTexture(), filter,
                                      address);
        RenderableTexture* specular = material->getSpecularMap();
        if (!specular)
            specular = CommonResourcesGL::WhiteMap;
        m_pContext->setUniformTexture(ShaderProgramGL::U_specularMap,
                                      ((RenderableTextureGL*)specular)->getTexture(), filter,
                                      address);
        RenderableTexture* normal = material->getNormalMap();
        if (!normal || !m_normalMapping)
            normal = CommonResourcesGL::DefaultNormalMap;
        m_pContext->setUniformTexture(ShaderProgramGL::U_normalMap,
                                      ((RenderableTextureGL*)normal)->getTexture(), filter,
                                      address);
        mesh->renderSegment(i);
        ++g_renderStats.drawSegments;
    }
    glActiveTexture(GL_TEXTURE0);
    for (int i = 0; i < 10; ++i)
        glDisableVertexAttribArray(i);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

// 0x08128720: the primary light is shaded per pixel, up to six others per vertex.
void NotebookRendererGL::renderOpaquePass(const Camera& camera, const RenderVisitor& visitor)
{
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    const LightEntity* primary = 0;
    int numVertexLights = 0;
    const Matrix4x3& worldToView = camera.getWorldToLocalMatrix();
    for (int i = 0; i < visitor.m_lights.size(); ++i)
    {
        const LightEntity* light = visitor.m_lights[i];
        if (light->getPrimaryLight())
        {
            primary = light;
            continue;
        }
        if (numVertexLights >= MaxVertexLights)
            continue;
        m_vlightColor[numVertexLights] = light->getLightColor();
        m_vlightPosition[numVertexLights] =
            worldToView.transformPoint(light->getNode()->getLocalToWorldMatrix().pos);
        m_vlightInvRange[numVertexLights] = 1.0f / light->getLightRange();
        ++numVertexLights;
    }
    for (int i = numVertexLights; i < MaxVertexLights; ++i)
    {
        m_vlightColor[i].set(0, 0, 0);
        m_vlightPosition[i].set(0, 0, 0);
        m_vlightInvRange[i] = 1.0f;
    }
    for (int i = 0; i < visitor.m_meshes.size(); ++i)
        renderMesh(camera, *visitor.m_meshes[i], primary, OpaquePass);
}

// 0x081288f0
void NotebookRendererGL::renderTransparentPass(const Camera& camera, const RenderVisitor& visitor)
{
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    static Array<SortItem> transparents;
    transparents.clear();
    transparents.reserve(visitor.m_meshes.size() + visitor.m_particles.size());
    const Matrix4x3& worldToView = camera.getWorldToLocalMatrix();
    for (int i = 0; i < visitor.m_meshes.size(); ++i)
    {
        MeshEntity* entity = visitor.m_meshes[i];
        RenderableMeshGL* mesh = (RenderableMeshGL*)entity->getMesh();
        for (int s = 0; s < mesh->getNumSegments(); ++s)
        {
            if (segmentMaterial(*entity, s)->getBlendMode() != Material::Opaque)
            {
                SortItem item;
                item.entity = entity;
                const Vec3& p = entity->getNode()->getLocalToWorldMatrix().pos;
                item.depth = worldToView.x.z * p.x + worldToView.y.z * p.y + worldToView.z.z * p.z +
                             worldToView.pos.z + entity->getSortOffset();
                transparents.push_back(item);
                break;
            }
        }
    }
    for (int i = 0; i < visitor.m_particles.size(); ++i)
    {
        ParticleEntity* entity = visitor.m_particles[i];
        SortItem item;
        item.entity = entity;
        const Vec3& p = entity->getNode()->getLocalToWorldMatrix().pos;
        item.depth = worldToView.x.z * p.x + worldToView.y.z * p.y + worldToView.z.z * p.z +
                     worldToView.pos.z + entity->getSortOffset();
        transparents.push_back(item);
    }
    std::sort(transparents.begin(), transparents.end());
    for (int i = 0; i < transparents.size(); ++i)
    {
        RenderEntity* entity = transparents[i].entity;
        if (entity->getEntityType() == RenderEntity::MeshEntityType)
            renderMesh(camera, *(MeshEntity*)entity, 0, TransparentPass);
        else
            m_pParticleRenderer->renderParticleSystem(camera, *(ParticleEntity*)entity,
                                                      m_diffuseMapping);
    }
}

} // namespace engine
