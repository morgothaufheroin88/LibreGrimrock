// Reconstructed from Grimrock.bin.x86 LightPrePassRendererGL.cpp.
#include "engine/LightPrePassRendererGL.h"
#include "core/Exception.h"
#include "core/Math.h"
#include "core/MersenneTwister.h"
#include "core/Prim.h"
#include "engine/Camera.h"
#include "engine/ParticleSystem.h"
#include "engine/ParticleSystemRendererGL.h"
#include "engine/RenderEntity.h"
#include "engine/Renderer.h"
#include <algorithm>
#include <cmath>

namespace engine
{

using namespace core;

namespace
{
// 8 bytes: entity and its view space depth.
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

static void setProgram(ShaderProgramGL*& slot, ShaderProgramGL* program)
{
    if (slot != program)
    {
        delete slot;
        slot = program;
    }
}
static void setTexture2D(Texture2DGL*& slot, Texture2DGL* texture)
{
    if (slot != texture)
    {
        delete slot;
        slot = texture;
    }
}
static const Material* segmentMaterial(const MeshEntity& entity, int segment)
{
    if (segment < entity.getMaterials().size())
        return entity.getMaterials()[segment].get();
    return Material::Default.get();
}

// 0x0811b990
LightPrePassRendererGL::LightPrePassRendererGL(RenderContextGL* context, int width, int height)
    : m_diffuseMapping(true), m_normalMapping(true), m_renderMeshes(true), m_renderShadows(true),
      m_textureFilter(0), m_shadowQuality(0), m_pContext(context), m_width(width), m_height(height),
      m_viewportX(0), m_viewportY(0), m_viewportWidth(0), m_viewportHeight(0),
      m_pParticleRenderer(0), m_depthStencilBuffer(0), m_pGeometryBuffer(0), m_pGlossinessBuffer(0),
      m_pLightBuffer(0), m_pColorBuffer(0), m_pDefaultShader(0), m_pAmbientLightProgram(0),
      m_pPointLightProgram(0), m_pPointLightShadowProgram(0), m_pDirectionalLightProgram(0),
      m_pStencilProgram(0), m_pBlurCubeMapProgram(0)
{
    m_pParticleRenderer = new ParticleSystemRendererGL(m_pContext);
    m_pDefaultShader = new RenderableShaderGL;
    m_pDefaultShader->initSurfaceShader(0, 0, 0, 0, 0);
    setProgram(m_pAmbientLightProgram,
               new ShaderProgramGL("shaders/gl/AmbientLight.vsh", "shaders/gl/AmbientLight.fsh"));
    setProgram(m_pPointLightProgram,
               new ShaderProgramGL("shaders/gl/PointLight.vsh", "shaders/gl/PointLight.fsh"));
    setProgram(
        m_pPointLightShadowProgram,
        new ShaderProgramGL("shaders/gl/PointLight.vsh", "shaders/gl/PointLight_CastShadow.fsh"));
    setProgram(m_pDirectionalLightProgram, new ShaderProgramGL("shaders/gl/DirectionalLight.vsh",
                                                               "shaders/gl/DirectionalLight.fsh"));
    setProgram(m_pStencilProgram,
               new ShaderProgramGL("shaders/gl/Stencil.vsh", "shaders/gl/Stencil.fsh"));
    setProgram(m_pBlurCubeMapProgram,
               new ShaderProgramGL("shaders/gl/BlurCubeMap.vsh", "shaders/gl/BlurCubeMap.fsh"));
    resizeRenderBuffers(width, height);
    // shadow cube maps for sizes 16..256, indexed by log2(size)
    for (int i = 0, size = 1; i < 9; ++i, size <<= 1)
    {
        if (size < 16)
        {
            m_shadowMaps.push_back(0);
            m_blurTemps.push_back(0);
            m_shadowDepthBuffers.push_back(0);
        }
        else
        {
            m_shadowMaps.push_back(new TextureCubeGL(size, 1, GL_RG16F));
            m_blurTemps.push_back(new TextureCubeGL(size, 1, GL_RG16F));
            GLuint depth;
            glGenRenderbuffers(1, &depth);
            glBindRenderbuffer(GL_RENDERBUFFER, depth);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size, size);
            m_shadowDepthBuffers.push_back(depth);
        }
    }
}

// 0x0811a5c0
LightPrePassRendererGL::~LightPrePassRendererGL()
{
    glDeleteRenderbuffers(1, &m_depthStencilBuffer);
    for (int i = 0; i < m_shadowMaps.size(); ++i)
        delete m_shadowMaps[i];
    for (int i = 0; i < m_blurTemps.size(); ++i)
        delete m_blurTemps[i];
    for (int i = 0; i < m_shadowDepthBuffers.size(); ++i)
        glDeleteRenderbuffers(1, &m_shadowDepthBuffers[i]);
    delete m_pBlurCubeMapProgram;
    delete m_pStencilProgram;
    delete m_pDirectionalLightProgram;
    delete m_pPointLightShadowProgram;
    delete m_pPointLightProgram;
    delete m_pAmbientLightProgram;
    delete m_pDefaultShader;
    delete m_pColorBuffer;
    delete m_pLightBuffer;
    delete m_pGlossinessBuffer;
    delete m_pGeometryBuffer;
    delete m_pParticleRenderer;
}

// 0x0811a320
void LightPrePassRendererGL::setViewport(int x, int y, int width, int height)
{
    m_viewportX = x;
    m_viewportY = y;
    m_viewportWidth = width;
    m_viewportHeight = height;
}

// 0x0811a380: four half float buffers and a packed depth stencil buffer.
void LightPrePassRendererGL::resizeRenderBuffers(int width, int height)
{
    if (m_depthStencilBuffer)
        glDeleteRenderbuffers(1, &m_depthStencilBuffer);
    glGenRenderbuffers(1, &m_depthStencilBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthStencilBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    setTexture2D(m_pGeometryBuffer, new Texture2DGL(width, height, 1, GL_RGBA16F, GL_NEAREST,
                                                    GL_NEAREST, GL_CLAMP_TO_EDGE));
    setTexture2D(m_pGlossinessBuffer, new Texture2DGL(width, height, 1, GL_RGBA16F, GL_NEAREST,
                                                      GL_NEAREST, GL_CLAMP_TO_EDGE));
    setTexture2D(m_pLightBuffer, new Texture2DGL(width, height, 1, GL_RGBA16F, GL_NEAREST,
                                                 GL_NEAREST, GL_CLAMP_TO_EDGE));
    setTexture2D(m_pColorBuffer, new Texture2DGL(width, height, 1, GL_RGBA16F, GL_NEAREST,
                                                 GL_NEAREST, GL_CLAMP_TO_EDGE));
    m_width = width;
    m_height = height;
}

// Viewport in GL window coordinates (y up).
static void setViewportGL(int x, int y, int w, int h, int height)
{
    glViewport(x, height - y - h, w, h);
    checkGLErrors("glViewport");
}

// 0x0811dde0: normals + depth and glossiness into two targets.
void LightPrePassRendererGL::renderGeometryPass(const Camera& camera, const RenderVisitor& visitor)
{
    m_pContext->setRenderTarget(m_pGeometryBuffer, m_pGlossinessBuffer, m_depthStencilBuffer,
                                m_depthStencilBuffer);
    setViewportGL(m_viewportX, m_viewportY, m_viewportWidth, m_viewportHeight, m_height);
    static constexpr GLenum buffers[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, buffers);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glClearColor(0, 0, 0, 0);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glActiveTexture(GL_TEXTURE0);
    glAlphaFunc(GL_GREATER, 0.5f);
    for (int i = 0; i < visitor.m_meshes.size(); ++i)
        renderMesh(camera, *visitor.m_meshes[i], GeometryPass);
    glDrawBuffers(1, buffers);
    for (int i = 0; i < 10; ++i)
        glDisableVertexAttribArray(i);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

// 0x08121ca0: accumulates all lights into the light buffer.
void LightPrePassRendererGL::renderLightPass(const Camera& camera, const RenderVisitor& visitor)
{
    m_pContext->setRenderTarget(m_pLightBuffer, 0, m_depthStencilBuffer, m_depthStencilBuffer);
    setViewportGL(m_viewportX, m_viewportY, m_viewportWidth, m_viewportHeight, m_height);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    for (int i = 0; i < visitor.m_lights.size(); ++i)
    {
        const LightEntity& light = *visitor.m_lights[i];
        switch (light.getLightType())
        {
        case LightEntity::Directional:
            renderDirectionalLight(camera, light);
            break;
        case LightEntity::Point:
            if (!light.getCastShadow() || !m_renderShadows)
            {
                renderPointLight(camera, light, 0);
            }
            else
            {
                TextureCubeGL* shadowMap;
                if (!light.getStaticShadowMap())
                {
                    // smallest shadow map at least as large as requested
                    int index = m_shadowMaps.size() - 1;
                    for (int k = 0; k < m_shadowMaps.size(); ++k)
                    {
                        if (m_shadowMaps[k] &&
                            light.getShadowMapSize() <= m_shadowMaps[k]->getWidth())
                        {
                            index = k;
                            break;
                        }
                    }
                    shadowMap = m_shadowMaps[index];
                    if (m_shadowQuality < 1)
                    {
                        renderPointLightShadowMap(light, shadowMap, m_shadowDepthBuffers[index],
                                                  false);
                    }
                    else
                    {
                        renderPointLightShadowMap(light, m_blurTemps[index],
                                                  m_shadowDepthBuffers[index], false);
                        blurCubeMapGPU(m_blurTemps[index], shadowMap, 3.0f * PI / 180.0f);
                    }
                    m_pContext->setRenderTarget(m_pLightBuffer, 0, m_depthStencilBuffer,
                                                m_depthStencilBuffer);
                    setViewportGL(m_viewportX, m_viewportY, m_viewportWidth, m_viewportHeight,
                                  m_height);
                }
                else
                {
                    shadowMap = (TextureCubeGL*)((RenderableTextureGL*)light.getStaticShadowMap())
                                    ->getTexture();
                }
                renderPointLight(camera, light, shadowMap);
            }
            break;
        case LightEntity::Ambient:
            renderAmbientLight(camera, light);
            break;
        default:
            break;
        }
    }
}

// 0x0811d590: shades the opaque meshes with the light buffer.
void LightPrePassRendererGL::renderMaterialPass(const Camera& camera, const RenderVisitor& visitor)
{
    m_pContext->setRenderTarget(m_pColorBuffer, 0, m_depthStencilBuffer, m_depthStencilBuffer);
    setViewportGL(m_viewportX, m_viewportY, m_viewportWidth, m_viewportHeight, m_height);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    for (int i = 0; i < visitor.m_meshes.size(); ++i)
        renderMesh(camera, *visitor.m_meshes[i], MaterialPass);
}

// 0x0811d680: blended meshes and particle systems sorted back to front.
void LightPrePassRendererGL::renderTransparentPass(const Camera& camera,
                                                   const RenderVisitor& visitor)
{
    m_pContext->setRenderTarget(m_pColorBuffer, 0, m_depthStencilBuffer, m_depthStencilBuffer);
    setViewportGL(m_viewportX, m_viewportY, m_viewportWidth, m_viewportHeight, m_height);
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
            renderMesh(camera, *(MeshEntity*)entity, TransparentPass);
        else
            m_pParticleRenderer->renderParticleSystem(camera, *(ParticleEntity*)entity,
                                                      m_diffuseMapping);
    }
}

// 0x0811c910: draws the segments of one mesh entity for the given pass.
void LightPrePassRendererGL::renderMesh(const Camera& camera, const MeshEntity& entity,
                                        RenderPass pass)
{
    RenderableMeshGL* mesh = (RenderableMeshGL*)entity.getMesh();
    bool skinned = mesh->isSkinned() && entity.getSkeleton() != 0;
    bool normalMapped = false;
    if (pass == GeometryPass && mesh->hasTangents() && m_normalMapping)
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
    static constexpr int filterModes[3] = {Material::Linear_MipNearest, Material::Linear_MipLinear,
                                           Material::Anisotropic};
    int filter = filterModes[m_textureFilter];
    for (int i = 0; i < mesh->getNumSegments(); ++i)
    {
        const Material* material = segmentMaterial(entity, i);
        bool transparent = pass == TransparentPass;
        if ((material->getBlendMode() == Material::Opaque) == transparent)
            continue;
        // pick the shader variant
        int shaderPass, variant = skinned ? RenderableShaderGL::Skinning : 0;
        if (pass == GeometryPass)
        {
            shaderPass = RenderableShaderGL::GeometryPass;
            if (material->getAlphaTest())
                variant |= RenderableShaderGL::AlphaTest;
            if (normalMapped)
                variant |= RenderableShaderGL::NormalMap;
        }
        else if (pass == MaterialPass && material->getLighting())
        {
            shaderPass = RenderableShaderGL::MaterialPass;
        }
        else
        {
            shaderPass = RenderableShaderGL::UnlitPass;
        }
        RenderableShaderGL* shader = (RenderableShaderGL*)material->getShader();
        if (!shader)
            shader = m_pDefaultShader;
        m_pContext->useProgram(shader->getProgram(shaderPass, variant));
        ShaderProgramGL* program = m_pContext->getProgram();

        const Matrix4x3& localToWorld = entity.getNode()->getLocalToWorldMatrix();
        Matrix4x4 mvp = RenderContextGL::sm_d3dToGLProj *
                        (camera.getViewProjectionMatrix() * Matrix4x4(localToWorld));
        glUniformMatrix4fv(program->getUniform(ShaderProgramGL::U_modelViewProj), 1, GL_FALSE,
                           mvp.m);
        Matrix4x4 modelView(camera.getWorldToLocalMatrix() * localToWorld);
        glUniformMatrix4fv(program->getUniform(ShaderProgramGL::U_modelView), 1, GL_FALSE,
                           modelView.m);
        if (skinned)
            m_pContext->setSkinningMatrices(entity);
        glUniform2f(glGetUniformLocation(program->getProgram(), "invScreenSize"), 1.0f / m_width,
                    1.0f / m_height);
        const Vec3& emissive = entity.getEmissiveColor();
        glUniform3f(glGetUniformLocation(program->getProgram(), "emissiveColor"), emissive.x,
                    emissive.y, emissive.z);
        glUniform1f(program->getUniform(ShaderProgramGL::U_glossiness), material->getGlossiness());
        if (material->getParamCount() > 0)
            m_pContext->setShaderParams(*material);
        if (material->getDoubleSided())
            glDisable(GL_CULL_FACE);
        else
            glEnable(GL_CULL_FACE);
        m_pContext->setBlendMode(material->getBlendMode());
        if (pass == MaterialPass && material->getAlphaTest())
            glEnable(GL_ALPHA_TEST);
        else
            glDisable(GL_ALPHA_TEST);

        int address = material->getTextureAddressMode();
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
        m_pContext->setUniformTexture("lightBuffer", m_pLightBuffer, -1, -1);
        mesh->renderSegment(i);
        ++g_renderStats.drawSegments;
    }
    glActiveTexture(GL_TEXTURE0);
    for (int i = 0; i < 10; ++i)
        glDisableVertexAttribArray(i);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

// 0x0811aea0
void LightPrePassRendererGL::renderAmbientLight(const Camera& camera, const LightEntity& light)
{
    m_pContext->useProgram(m_pAmbientLightProgram);
    const Vec3& c = light.getLightColor();
    glUniform3f(m_pAmbientLightProgram->getUniform(ShaderProgramGL::U_lightColor), c.x, c.y, c.z);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_ALPHA_TEST);
    m_pContext->setBlendMode(RenderContextGL::Blend_Additive);
    m_pContext->drawRect();
}

// 0x081218c0
void LightPrePassRendererGL::renderDirectionalLight(const Camera& camera, const LightEntity& light)
{
    m_pContext->useProgram(m_pDirectionalLightProgram);
    GLuint program = m_pDirectionalLightProgram->getProgram();
    m_pContext->setUniformTexture("geometryBuffer", m_pGeometryBuffer, -1, -1);
    m_pContext->setUniformTexture("glossinessBuffer", m_pGlossinessBuffer, -1, -1);
    glUniform2f(glGetUniformLocation(program, "invScreenSize"),
                1.0f / m_pGeometryBuffer->getWidth(), 1.0f / m_pGeometryBuffer->getHeight());
    glUniform1f(glGetUniformLocation(program, "invNear"), 1.0f / camera.getNear());
    glUniformMatrix4fv(glGetUniformLocation(program, "invProjectionMatrix"), 1, GL_FALSE,
                       camera.getInverseProjectionMatrix().m);
    // light z axis in view space
    Matrix4x3 worldToView(camera.getWorldToLocalMatrix().rotation());
    Vec3 dir = worldToView.transformPoint(light.getNode()->getLocalToWorldMatrix().z);
    glUniform3f(glGetUniformLocation(program, "lightDirection"), dir.x, dir.y, dir.z);
    const Vec3& c = light.getLightColor();
    glUniform3f(glGetUniformLocation(program, "lightColor"), c.x, c.y, c.z);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_ALPHA_TEST);
    m_pContext->setBlendMode(RenderContextGL::Blend_Additive);
    m_pContext->drawRect();
}

// 0x0811a340
void LightPrePassRendererGL::renderSpotLight(const Camera& camera, const LightEntity& light) {}

// 0x0811a8c0: scissor rectangle of a sphere in view space (Lengyel's method).
int LightPrePassRendererGL::setupPointLightScissorRect(const Camera& camera, const Vec3& lightPos,
                                                       float radius)
{
    float radiusSqr = radius * radius;
    int viewportWidth = m_viewportWidth, viewportHeight = m_viewportHeight;
    float negLightZ = -lightPos.z;
    float lightZSqr = lightPos.z * lightPos.z;
    const float* invProj = camera.getInverseProjectionMatrix().m;
    // corner of the near plane in view space
    float invW = 1.0f / (invProj[3] + invProj[7] + invProj[15]);
    float nearZ = (invProj[2] + invProj[6] + invProj[14]) * invW;
    int xmin = 0, xmax = viewportWidth, ymin = 0, ymax = viewportHeight;
    {
        float denom = lightZSqr + lightPos.x * lightPos.x;
        float discriminant = lightPos.x * lightPos.x * radiusSqr - denom * (radiusSqr - lightZSqr);
        if (discriminant >= 0.0f)
        {
            float xScale = nearZ / ((invProj[0] + invProj[4] + invProj[12]) * invW);
            float sqrtDisc = std::sqrt(discriminant);
            float nx1 = (sqrtDisc + radius * lightPos.x) / denom,
                  nx2 = (radius * lightPos.x - sqrtDisc) / denom;
            float nz1 = (radius - nx1 * lightPos.x) / negLightZ,
                  nz2 = (radius - nx2 * lightPos.x) / negLightZ;
            float pz1 = (denom - radiusSqr) / (negLightZ - (nz1 / nx1) * lightPos.x);
            float pz2 = (denom - radiusSqr) / (negLightZ - (nz2 / nx2) * lightPos.x);
            if (pz1 < 0.0f)
            {
                int x = (int)lrintf(viewportWidth * ((nz1 * xScale) / nx1 + 1.0f) * 0.5f);
                if (lightPos.x <= -pz1 * nz1 / nx1)
                {
                    if (x < xmax)
                        xmax = x;
                }
                else if (x > xmin)
                {
                    xmin = x;
                }
            }
            if (pz2 < 0.0f)
            {
                int x = (int)lrintf(viewportWidth * ((nz2 * xScale) / nx2 + 1.0f) * 0.5f);
                if (lightPos.x <= -pz2 * nz2 / nx2)
                {
                    if (x < xmax)
                        xmax = x;
                }
                else if (x > xmin)
                {
                    xmin = x;
                }
            }
        }
    }
    {
        float denom = lightZSqr + lightPos.y * lightPos.y;
        float discriminant = lightPos.y * lightPos.y * radiusSqr - (radiusSqr - lightZSqr) * denom;
        if (discriminant >= 0.0f)
        {
            float yScale = nearZ / ((invProj[1] + invProj[5] + invProj[13]) * invW);
            float sqrtDisc = std::sqrt(discriminant);
            float ny1 = (sqrtDisc + radius * lightPos.y) / denom,
                  ny2 = (radius * lightPos.y - sqrtDisc) / denom;
            float nz1 = (radius - ny1 * lightPos.y) / negLightZ,
                  nz2 = (radius - ny2 * lightPos.y) / negLightZ;
            float pz1 = (denom - radiusSqr) / (negLightZ - (nz1 / ny1) * lightPos.y);
            float pz2 = (denom - radiusSqr) / (negLightZ - (nz2 / ny2) * lightPos.y);
            if (pz1 < 0.0f)
            {
                int y = (int)lrintf(viewportHeight * ((yScale * nz1) / ny1 + 1.0f) * 0.5f);
                if (lightPos.y <= -pz1 * nz1 / ny1)
                {
                    if (y < ymax)
                        ymax = y;
                }
                else if (y > ymin)
                {
                    ymin = y;
                }
            }
            if (pz2 < 0.0f)
            {
                int y = (int)lrintf(viewportHeight * ((yScale * nz2) / ny2 + 1.0f) * 0.5f);
                if (lightPos.y <= -pz2 * nz2 / ny2)
                {
                    if (y < ymax)
                        ymax = y;
                }
                else if (y > ymin)
                {
                    ymin = y;
                }
            }
        }
    }
    int height = ymax - ymin;
    if (height <= 0)
        return 0;
    int width = xmax - xmin > 0 ? xmax - xmin : 0;
    int area = height * width;
    if (area == 0)
        return 0;
    if (xmin >= viewportWidth || ymin >= viewportHeight)
        return 0;
    if (viewportWidth * viewportHeight - area == 0)
    {
        m_pContext->setScissorTest(false);
        return viewportWidth * viewportHeight;
    }
    int originX = m_viewportX, originY = m_height - viewportHeight - m_viewportY;
    glEnable(GL_SCISSOR_TEST);
    glScissor(xmin + originX, ymin + originY, xmax - xmin, ymax - ymin);
    checkGLErrors("glScissor");
    return area;
}

// 0x0811af70: marks the pixels inside the light sphere in the stencil buffer.
void LightPrePassRendererGL::renderPointLightStencil(const Camera& camera, const LightEntity& light)
{
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    m_pContext->setBlendMode(RenderContextGL::Blend_Opaque);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 0, 0xffffffff);
    glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP, GL_KEEP);
    glStencilOpSeparate(GL_BACK, GL_KEEP, GL_INCR_WRAP, GL_KEEP);
    m_pContext->useProgram(m_pStencilProgram);
    float range = light.getLightRange();
    Matrix4x3 m;
    m.makeIdentity();
    m.pos = light.getNode()->getLocalToWorldMatrix().pos;
    m.x *= range;
    m.y *= range;
    m.z *= range;
    Matrix4x4 mvp =
        RenderContextGL::sm_d3dToGLProj * (camera.getViewProjectionMatrix() * Matrix4x4(m));
    glUniformMatrix4fv(glGetUniformLocation(m_pStencilProgram->getProgram(), "viewProjMatrix"), 1,
                       GL_FALSE, mvp.m);
    m_pContext->drawMesh(*CommonResourcesGL::SphereMesh);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

// 0x0811b240
void LightPrePassRendererGL::renderPointLight(const Camera& camera, const LightEntity& light,
                                              TextureCubeGL* shadowMap)
{
    float range = light.getLightRange();
    const Matrix4x3& worldToView = camera.getWorldToLocalMatrix();
    const Matrix4x3& lightToWorld = light.getNode()->getLocalToWorldMatrix();
    Vec3 viewPos = worldToView.transformPoint(lightToWorld.pos);
    if (!setupPointLightScissorRect(camera, viewPos, range))
        return;
    // the stencil volume is only needed when the sphere is inside the far plane
    bool useStencil = viewPos.z + range < camera.getFar();
    if (useStencil)
        renderPointLightStencil(camera, light);
    ShaderProgramGL* prog = shadowMap ? m_pPointLightShadowProgram : m_pPointLightProgram;
    m_pContext->useProgram(prog);
    GLuint program = prog->getProgram();
    m_pContext->setUniformTexture("geometryBuffer", m_pGeometryBuffer, -1, -1);
    m_pContext->setUniformTexture("glossinessBuffer", m_pGlossinessBuffer, -1, -1);
    glUniform2f(glGetUniformLocation(program, "invScreenSize"), 1.0f / m_pLightBuffer->getWidth(),
                1.0f / m_pLightBuffer->getHeight());
    glUniform1f(glGetUniformLocation(program, "invNear"), 1.0f / camera.getNear());
    glUniformMatrix4fv(glGetUniformLocation(program, "invProjectionMatrix"), 1, GL_FALSE,
                       camera.getInverseProjectionMatrix().m);
    glUniform3f(glGetUniformLocation(program, "lightPosition"), viewPos.x, viewPos.y, viewPos.z);
    const Vec3& c = light.getLightColor();
    glUniform3f(prog->getUniform(ShaderProgramGL::U_lightColor), c.x, c.y, c.z);
    glUniform1f(glGetUniformLocation(program, "invLightRange"), range > 0.0f ? 1.0f / range : 0.0f);
    if (shadowMap)
    {
        // view space to light cube space, scaled by the range
        float invRange = 1.0f / range;
        Matrix4x3 m = camera.getLocalToWorldMatrix();
        m.pos -= lightToWorld.pos;
        m.x *= invRange;
        m.y *= invRange;
        m.z *= invRange;
        m.pos *= invRange;
        m_pContext->setUniformTexture("shadowCubeMap", shadowMap, -1, -1);
        Matrix4x4 shadowProj(m);
        glUniformMatrix4fv(glGetUniformLocation(program, "shadowProjMatrix"), 1, GL_FALSE,
                           shadowProj.m);
    }
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_ALPHA_TEST);
    m_pContext->setBlendMode(RenderContextGL::Blend_Additive);
    if (useStencil)
    {
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_EQUAL, 1, 0xff);
        glStencilOp(GL_KEEP, GL_ZERO, GL_ZERO);
        checkGLErrors("pointlight stencil");
    }
    m_pContext->drawRect();
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
}

// 0x0811dff0: depth of the six cube faces around a point light.
void LightPrePassRendererGL::renderPointLightShadowMap(const LightEntity& light,
                                                       TextureCubeGL* cube, GLuint depthBuffer,
                                                       bool staticOnly)
{
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_ALPHA_TEST);
    m_pContext->setBlendMode(RenderContextGL::Blend_Opaque);
    Matrix4x3 cameraMatrix;
    cameraMatrix.makeIdentity();
    cameraMatrix.pos = light.getNode()->getLocalToWorldMatrix().pos;
    static const Matrix3x3 faceRotations[6] = {
        Matrix3x3(Vec3(0, 0, -1), Vec3(0, 1, 0), Vec3(1, 0, 0)),
        Matrix3x3(Vec3(0, 0, 1), Vec3(0, 1, 0), Vec3(-1, 0, 0)),
        Matrix3x3(Vec3(1, 0, 0), Vec3(0, 0, -1), Vec3(0, 1, 0)),
        Matrix3x3(Vec3(1, 0, 0), Vec3(0, 0, 1), Vec3(0, -1, 0)),
        Matrix3x3(Vec3(1, 0, 0), Vec3(0, 1, 0), Vec3(0, 0, 1)),
        Matrix3x3(Vec3(-1, 0, 0), Vec3(0, 1, 0), Vec3(0, 0, -1)),
    };
    float range = light.getLightRange();
    Matrix4x4 proj;
    makePerspectiveProjectionMatrix(&proj, HALF_PI, 1.0f, 0.1f, range);
    ShadowMapVisitor visitor;
    for (int face = 0; face < 6; ++face)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_pContext->getFrameBuffer());
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                  depthBuffer);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, cube->getHandle(), 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, 0, 0);
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
            throw Exception("Could not set render targets (glCheckFramebufferStatus returned %d)",
                            status);
        glViewport(0, 0, cube->getWidth(), cube->getHeight());
        checkGLErrors("glViewport");
        glClearColor(1, 1, 1, 1);
        glClearDepth(1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        cameraMatrix.rotation() = faceRotations[face];
        Matrix4x3 view = cameraMatrix;
        view.invertOrthonormal();
        Plane planes[6];
        computeFrustumPlanes(planes, HALF_PI, 0.1f, range, cameraMatrix);
        visitor.gatherEntities(*light.getNode()->getScene(), planes, 6);
        // cube faces are stored upside down
        Matrix4x4 flipY;
        flipY.m[5] = -1.0f;
        Matrix4x4 viewProj = (proj * Matrix4x4(view)) * flipY;
        glFrontFace(GL_CCW);
        for (int i = 0; i < visitor.m_meshes.size(); ++i)
        {
            const MeshEntity& entity = *visitor.m_meshes[i];
            RenderableMeshGL* mesh = (RenderableMeshGL*)entity.getMesh();
            if (staticOnly && !(entity.getFlags() & MeshEntity::StaticShadow))
                continue;
            bool skinned = mesh->isSkinned() && entity.getSkeleton() != 0;
            mesh->activate();
            for (int s = 0; s < mesh->getNumSegments(); ++s)
            {
                const Material* material = mesh->getSegment(s).material.get();
                if (material->getBlendMode() != Material::Opaque)
                    continue;
                int variant = skinned ? RenderableShaderGL::Skinning : 0;
                if (material->getAlphaTest())
                    variant |= RenderableShaderGL::AlphaTest;
                m_pContext->useProgram(
                    m_pDefaultShader->getProgram(RenderableShaderGL::ShadowPass, variant));
                ShaderProgramGL* program = m_pContext->getProgram();
                glUniform1f(glGetUniformLocation(program->getProgram(), "invLightRange"),
                            1.0f / range);
                const Matrix4x3& localToWorld = entity.getNode()->getLocalToWorldMatrix();
                Matrix4x4 mvp =
                    RenderContextGL::sm_d3dToGLProj * (viewProj * Matrix4x4(localToWorld));
                glUniformMatrix4fv(program->getUniform(ShaderProgramGL::U_modelViewProj), 1,
                                   GL_FALSE, mvp.m);
                Matrix4x4 modelView(view * localToWorld);
                glUniformMatrix4fv(program->getUniform(ShaderProgramGL::U_modelView), 1, GL_FALSE,
                                   modelView.m);
                if (skinned)
                    m_pContext->setSkinningMatrices(entity);
                if (material->getDoubleSided())
                    glDisable(GL_CULL_FACE);
                else
                    glEnable(GL_CULL_FACE);
                if (material->getAlphaTest())
                {
                    glEnable(GL_ALPHA_TEST);
                    RenderableTexture* diffuse = material->getDiffuseMap();
                    if (!diffuse)
                        diffuse = CommonResourcesGL::WhiteMap;
                    m_pContext->setUniformTexture(
                        "diffuseMap", ((RenderableTextureGL*)diffuse)->getTexture(),
                        Material::Linear_MipLinear, material->getTextureAddressMode());
                }
                else
                {
                    glDisable(GL_ALPHA_TEST);
                }
                mesh->renderSegment(s);
                ++g_renderStats.shadowSegments;
            }
        }
        glFrontFace(GL_CW);
        checkGLErrors("pointlight shadow");
    }
    glDisable(GL_ALPHA_TEST);
    glActiveTexture(GL_TEXTURE0);
    for (int i = 0; i < 10; ++i)
        glDisableVertexAttribArray(i);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

// 0x0811f1b0
TextureCubeGL* LightPrePassRendererGL::renderStaticShadowMap(const LightEntity& light, int size)
{
    TextureCubeGL* cube = new TextureCubeGL(size, 1, GL_RG16F);
    GLuint depthBuffer = 0;
    for (int i = 0; i < m_shadowDepthBuffers.size(); ++i)
    {
        if (m_shadowMaps[i] && m_shadowMaps[i]->getWidth() == size)
            depthBuffer = m_shadowDepthBuffers[i];
    }
    if (!depthBuffer)
        throw Exception("No depth buffer for static shadow map");
    renderPointLightShadowMap(light, cube, depthBuffer, true);
    return cube;
}

// 0x081205b0: 16 jittered samples in a cone of blurAngle around each texel direction.
void LightPrePassRendererGL::blurCubeMapGPU(TextureCubeGL* source, TextureCubeGL* target,
                                            float blurAngle)
{
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    m_pContext->setBlendMode(RenderContextGL::Blend_Opaque);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_CULL_FACE);
    m_pContext->useProgram(m_pBlurCubeMapProgram);
    GLuint program = m_pBlurCubeMapProgram->getProgram();
    MersenneTwister rng;
    Vec3 samples[16];
    for (int i = 0; i < 16; ++i)
    {
        float r1 = rng.genrand_int32() * 2.3283064370807974e-10f;
        float r2 = rng.genrand_int32() * 2.3283064370807974e-10f;
        Matrix3x3 rot;
        rot.makeRotation(r2 * blurAngle - blurAngle * 0.5f, r1 * TWO_PI - PI, 0.0f, Matrix3x3::XYZ);
        samples[i] = rot.y;
    }
    m_pContext->setUniformTexture("tex", source, -1, -1);
    glUniform3fv(glGetUniformLocation(program, "samples"), 16, &samples[0].x);
    static const Matrix3x3 faceRotations[6] = {
        Matrix3x3(Vec3(0, 0, 1), Vec3(0, -1, 0), Vec3(-1, 0, 0)),
        Matrix3x3(Vec3(0, 0, -1), Vec3(0, -1, 0), Vec3(1, 0, 0)),
        Matrix3x3(Vec3(1, 0, 0), Vec3(0, 0, 1), Vec3(0, 1, 0)),
        Matrix3x3(Vec3(1, 0, 0), Vec3(0, 0, -1), Vec3(0, -1, 0)),
        Matrix3x3(Vec3(1, 0, 0), Vec3(0, -1, 0), Vec3(0, 0, 1)),
        Matrix3x3(Vec3(-1, 0, 0), Vec3(0, -1, 0), Vec3(0, 0, -1)),
    };
    for (int face = 0; face < 6; ++face)
    {
        Matrix3x3 rot = faceRotations[face];
        rot.transpose();
        glBindFramebuffer(GL_FRAMEBUFFER, m_pContext->getFrameBuffer());
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, target->getHandle(), 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, 0, 0);
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
            throw Exception("Could not set render targets (glCheckFramebufferStatus returned %d)",
                            status);
        glViewport(0, 0, target->getWidth(), target->getHeight());
        checkGLErrors("glViewport");
        glUniformMatrix3fv(glGetUniformLocation(program, "faceRot"), 1, GL_FALSE, &rot.x.x);
        m_pContext->drawRect();
    }
}

} // namespace engine
