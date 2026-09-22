// Reconstructed from grimrock2.exe NotebookRendererGL.cpp.
#include "engine/NotebookRendererGL.h"
#include "core/Profiler.h"
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
struct DrawItem
{
    unsigned int material;
    unsigned int program;
    unsigned int entity;
    unsigned int segment;
    bool operator<(const DrawItem& o) const
    {
        if (program != o.program)
            return program < o.program;
        if (material != o.material)
            return material < o.material;
        if (entity != o.entity)
            return entity < o.entity;
        return segment < o.segment;
    }
};
struct SortItem
{
    RenderEntity* entity;
    float depth;
    bool operator<(const SortItem& o) const
    {
        return depth > o.depth;
    }
};
// point lights nearest to the camera first
struct LightItem
{
    LightEntity* light;
    float distanceSqr;
    bool operator<(const LightItem& o) const
    {
        return distanceSqr < o.distanceSqr;
    }
};
} // namespace

static const Material* segmentMaterial(const MeshEntity& entity, int segment)
{
    if (segment < entity.getMaterials().size())
        return entity.getMaterials()[segment].get();
    return CommonResourcesGL::DefaultMaterial;
}
static TextureGL* textureOf(RenderableTexture* texture)
{
    return ((RenderableTextureGL*)texture)->getTexture();
}

// 0x004ede30
NotebookRendererGL::NotebookRendererGL(RenderContextGL* context)
    : m_diffuseMapping(true), m_normalMapping(true), m_fogColor(0, 0, 0), m_fogStart(0),
      m_fogEnd(0), m_width(0), m_height(0), m_pContext(context), m_depthBuffer(0),
      m_pParticleRenderer(0), m_pShader(0), m_ambientLight(0, 0, 0), m_dirlightColor(0, 0, 0),
      m_dirlightDirection(0, 0, 0), m_lightColor(0, 0, 0), m_lightPosition(0, 0, 0),
      m_invLightRange(0)
{
    for (int i = 0; i < MaxVertexLights; ++i)
    {
        m_vlightColor[i].set(0, 0, 0);
        m_vlightPosition[i] = Vec4(0, 0, 0, 0);
    }
    m_pParticleRenderer = new ParticleSystemRendererGL(context);
    m_pShader = new RenderableShaderGL;
    m_pShader->initNotebookRendererShader(0, 0, 0);
}
// 0x004edfd0
NotebookRendererGL::~NotebookRendererGL()
{
    delete m_pShader;
    delete m_pParticleRenderer;
    if (m_depthBuffer)
        glDeleteRenderbuffers(1, &m_depthBuffer);
}
// 0x004edb60
void NotebookRendererGL::resizeRenderBuffers(int width, int height)
{
    if (m_depthBuffer)
        glDeleteRenderbuffers(1, &m_depthBuffer);
    glGenRenderbuffers(1, &m_depthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    m_width = width;
    m_height = height;
}

// Oblique near plane clipping (Lengyel) of the D3D style projection.
static Matrix4x4 obliqueProjection(const Camera& camera)
{
    Matrix4x4 proj = camera.getProjectionMatrix();
    const Plane& worldPlane = camera.getUserClipPlane(0);
    Vec3 n = camera.getWorldToLocalMatrix().rotation().transform(worldPlane.normal);
    float d = worldPlane.d - dot(worldPlane.normal, camera.getLocalToWorldMatrix().pos);
    Vec4 c(n.x, n.y, n.z, -d);
    c = c * (1.0f / fabsf(c.z));
    if (c.z < 0.0f)
        c = c * -1.0f;
    Vec4 q = camera.getInverseProjectionMatrix().transform(
        Vec4(c.x < 0.0f ? -1.0f : 1.0f, c.y < 0.0f ? -1.0f : 1.0f, 1.0f, 1.0f));
    Vec4 m = c * (1.0f / dot(c, q));
    proj.m[2] = m.x;
    proj.m[6] = m.y;
    proj.m[10] = m.z;
    proj.m[14] = m.w;
    return proj;
}

// 0x004ee9d0
void NotebookRendererGL::renderOpaquePass(const Camera& camera, const RenderVisitor& visitor)
{
    ProfileScope profile("_RenderOpaquePass");
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    // the ambient light is the average of the three hemisphere colours; the last
    // directional light and the primary point light win, the other point lights become
    // vertex lights nearest first
    static Array<LightItem> pointLights;
    pointLights.clear();
    m_ambientLight.set(0, 0, 0);
    m_dirlightColor.set(0, 0, 0);
    m_dirlightDirection.set(0, 0, 0);
    m_lightColor.set(0, 0, 0);
    m_lightPosition.set(0, 0, 0);
    m_invLightRange = 1.0f;
    const Matrix4x3& worldToView = camera.getWorldToLocalMatrix();
    const Vec3& eye = camera.getLocalToWorldMatrix().pos;
    for (int i = 0; i < visitor.m_lights.size(); ++i)
    {
        LightEntity* light = visitor.m_lights[i];
        if (light->getPrimaryLight())
        {
            m_lightColor = light->getLightColor();
            m_lightPosition =
                worldToView.transformPoint(light->getNode()->getLocalToWorldMatrix().pos);
            m_invLightRange = 1.0f / light->getLightRange();
            continue;
        }
        switch (light->getLightType())
        {
        case LightEntity::Ambient:
            m_ambientLight +=
                (light->getLightColor() + light->getLightColor2() + light->getLightColor3()) *
                (1.0f / 3.0f);
            break;
        case LightEntity::Directional:
            m_dirlightColor = light->getLightColor();
            m_dirlightDirection =
                worldToView.rotation().transform(light->getNode()->getLocalToWorldMatrix().z);
            break;
        case LightEntity::Point:
        {
            LightItem item;
            item.light = light;
            Vec3 d = light->getNode()->getLocalToWorldMatrix().pos - eye;
            item.distanceSqr = dot(d, d);
            pointLights.push_back(item);
            break;
        }
        default:
            break;
        }
    }
    std::sort(pointLights.begin(), pointLights.end());
    for (int i = 0; i < MaxVertexLights; ++i)
    {
        if (i < pointLights.size())
        {
            LightEntity* light = pointLights[i].light;
            m_vlightColor[i] = light->getLightColor();
            Vec3 p = worldToView.transformPoint(light->getNode()->getLocalToWorldMatrix().pos);
            m_vlightPosition[i] = Vec4(p, 1.0f / (light->getLightRange() * 0.5f));
        }
        else
        {
            m_vlightColor[i].set(0, 0, 0);
            m_vlightPosition[i] = Vec4(0, 0, 0, 0);
        }
    }
    if (camera.getUserClipPlaneMask() == 1)
        m_projection = obliqueProjection(camera);
    renderMeshes(camera, visitor.m_meshes.data(), visitor.m_meshes.size(), OpaquePass);
}

// 0x004ef030
void NotebookRendererGL::renderTransparentPass(const Camera& camera, const RenderVisitor& visitor,
                                               int pass)
{
    ProfileScope profile("_RenderTransparentPass");
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
            if (segmentMaterial(*entity, s)->getBlendMode() == Material::Opaque)
                continue;
            int entityPass = (entity->getRenderHack() & 2) ? TransparentPass2 : TransparentPass1;
            if (entityPass == pass)
            {
                SortItem item;
                item.entity = entity;
                const Vec3& p = entity->getNode()->getLocalToWorldMatrix().pos;
                item.depth = worldToView.x.z * p.x + worldToView.y.z * p.y + worldToView.z.z * p.z +
                             worldToView.pos.z + entity->getSortOffset();
                transparents.push_back(item);
            }
            break;
        }
    }
    if (pass == TransparentPass1)
    {
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
    }
    std::sort(transparents.begin(), transparents.end());
    Matrix4x4 identity;
    for (int i = 0; i < transparents.size(); ++i)
    {
        RenderEntity* entity = transparents[i].entity;
        if (entity->getEntityType() == RenderEntity::MeshEntityType)
        {
            MeshEntity* mesh = (MeshEntity*)entity;
            renderMeshes(camera, &mesh, 1, TransparentPass1);
        }
        else
        {
            m_pParticleRenderer->renderParticleSystem(camera, *(ParticleEntity*)entity,
                                                      m_diffuseMapping, identity);
        }
    }
}

// 0x004ee0d0
void NotebookRendererGL::renderMeshes(const Camera& camera, MeshEntity* const* meshes, int count,
                                      int pass)
{
    Matrix4x4 proj =
        RenderContextGL::sm_d3dToGLProj *
        (camera.getUserClipPlaneMask() == 1 ? m_projection : camera.getProjectionMatrix());
    static Array<DrawItem> items;
    items.clear();
    items.reserve(count);
    for (int e = 0; e < count; ++e)
    {
        const MeshEntity& entity = *meshes[e];
        RenderableMeshGL* mesh = (RenderableMeshGL*)entity.getMesh();
        bool skinned = entity.isSkinned() && mesh->isSkinned();
        bool tangents = mesh->hasTangents() && m_normalMapping;
        for (int s = 0; s < mesh->getNumSegments(); ++s)
        {
            const Material* material = segmentMaterial(entity, s);
            bool opaque = material->getBlendMode() == Material::Opaque;
            if (pass == OpaquePass ? !opaque : opaque)
                continue;
            bool unlit = !material->getLighting() || pass != OpaquePass;
            bool alphaTest = material->getAlphaTest() && pass == OpaquePass;
            int variant = RenderableShaderGL::NotebookPass |
                          (skinned ? RenderableShaderGL::Skinning : 0) |
                          (unlit ? RenderableShaderGL::UnlitPass : 0) |
                          (alphaTest ? RenderableShaderGL::AlphaTest : 0) |
                          (tangents && !unlit ? RenderableShaderGL::NormalMap : 0);
            RenderableShaderGL* shader = (RenderableShaderGL*)material->getShader();
            if (!shader)
                shader = m_pShader;
            DrawItem item;
            item.material = (unsigned int)material->getRegistryIndex();
            item.program = (unsigned int)shader->getProgram(variant)->getRegistryIndex();
            item.entity = (unsigned int)e;
            item.segment = (unsigned int)s;
            items.push_back(item);
        }
    }
    std::sort(items.begin(), items.end());

    ShaderProgramGL* currentProgram = 0;
    const Material* currentMaterial = 0;
    for (int i = 0; i < items.size(); ++i)
    {
        const DrawItem& item = items[i];
        const MeshEntity& entity = *meshes[item.entity];
        RenderableMeshGL* mesh = (RenderableMeshGL*)entity.getMesh();
        bool skinned = entity.isSkinned() && mesh->isSkinned();
        glBindVertexArray(mesh->getVertexArray());
        const Material* material = segmentMaterial(entity, item.segment);
        ShaderProgramGL* program = ShaderProgramGL::sm_programs[item.program];
        if (program != currentProgram)
        {
            m_pContext->useProgram(program);
            program->setUniform(ShaderProgramGL::U_proj, proj);
            program->setUniform(ShaderProgramGL::U_invScreenSize,
                                Vec2(1.0f / m_width, 1.0f / m_height));
            program->setUniform3v("g_vlightColor", &m_vlightColor[0].x, MaxVertexLights);
            program->setUniform4v("g_vlightPosition", &m_vlightPosition[0].x, MaxVertexLights);
            program->setUniform(ShaderProgramGL::U_lightColor, m_lightColor);
            program->setUniform("g_lightPosition", m_lightPosition);
            program->setUniform(ShaderProgramGL::U_invLightRange, m_invLightRange);
            program->setUniform("g_ambientLight", m_ambientLight);
            program->setUniform("g_dirlightColor", m_dirlightColor);
            program->setUniform("g_dirlightDirection", m_dirlightDirection);
            program->setUniform("g_fogColor", m_fogColor);
            float range = m_fogEnd - m_fogStart;
            program->setUniform("g_fogRangeParams", Vec2(1.0f / range, -m_fogStart / range));
            ++g_renderStats.bindShader;
            currentProgram = program;
        }
        Matrix4x4 modelView(camera.getWorldToLocalMatrix() *
                            entity.getNode()->getLocalToWorldMatrix());
        program->setUniform(ShaderProgramGL::U_modelView, modelView);
        if (skinned)
            m_pContext->setSkinningMatrices(entity);
        const Vec3& emissive = entity.getEmissiveColor();
        program->setUniform(ShaderProgramGL::U_emissiveColor, emissive);
        program->setUniform(ShaderProgramGL::U_texcoordScaleOffset,
                            mesh->getTexcoordScaleOffset(*material));
        if (material != currentMaterial)
        {
            if (material->getDoubleSided())
                glDisable(GL_CULL_FACE);
            else
                glEnable(GL_CULL_FACE);
            m_pContext->setBlendMode(material->getBlendMode());
            program->setUniform(ShaderProgramGL::U_glossiness, material->getGlossiness());
            RenderableTexture* diffuse = material->getDiffuseMap();
            if (!diffuse || !m_diffuseMapping)
                diffuse = CommonResourcesGL::GrayMap;
            RenderableTexture* specular = material->getSpecularMap();
            if (!specular)
                specular = CommonResourcesGL::WhiteMap;
            RenderableTexture* normal = material->getNormalMap();
            if (!normal || !m_normalMapping)
                normal = CommonResourcesGL::DefaultNormalMap;
            int address = material->getTextureAddressMode();
            m_pContext->setUniformTexture(ShaderProgramGL::U_diffuseMap, textureOf(diffuse), -1,
                                          address, 0);
            m_pContext->setUniformTexture(ShaderProgramGL::U_specularMap, textureOf(specular), -1,
                                          address, 1);
            m_pContext->setUniformTexture(ShaderProgramGL::U_normalMap, textureOf(normal), -1,
                                          address, 2);
            if (material->getParamCount() > 0)
                m_pContext->setShaderParams(*material, 3);
            ++g_renderStats.bindMaterial;
            currentMaterial = material;
        }
        mesh->drawSegment(item.segment);
        ++g_renderStats.drawSegments;
        g_renderStats.renderTriangles += mesh->getSegment(item.segment).primitiveCount;
    }
}

} // namespace engine
