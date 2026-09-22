// Reconstructed from grimrock2.exe LightPrePassRendererGL.cpp.
#include "engine/LightPrePassRendererGL.h"
#include "core/Exception.h"
#include "core/Math.h"
#include "core/MersenneTwister.h"
#include "core/Prim.h"
#include "core/Profiler.h"
#include "core/Sys.h"
#include "engine/Camera.h"
#include "engine/ParticleSystem.h"
#include "engine/ParticleSystemRendererGL.h"
#include "engine/PostGL.h"
#include "engine/RenderEntity.h"
#include "engine/Renderer.h"
#include "engine/Scene.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace engine
{

using namespace core;

namespace
{
// Sort key of a draw call: material index, program index, entity index and segment, so
// the state changes are minimised (the original packs them into a 64 bit integer).
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
// Whole meshes of the shadow pass: program then entity.
struct ShadowItem
{
    unsigned int program;
    unsigned int entity;
    bool operator<(const ShadowItem& o) const
    {
        return program != o.program ? program < o.program : entity < o.entity;
    }
};
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
// 0x006173b0 followers: the poisson disc of the directional light shadow lookup (17 taps)
constexpr float ShadowSamples[34] = {
    0.041056f,  -0.037428f, 0.010252f,  -0.110637f, -0.100439f, -0.133003f, -0.218438f,
    -0.040833f, -0.236171f, 0.146231f,  -0.091221f, 0.320609f,  0.173343f,  0.348119f,
    0.414432f,  0.160552f,  0.466236f,  -0.180621f, 0.247632f,  -0.497313f, -0.167238f,
    -0.587782f, -0.566811f, -0.350955f, -0.709925f, 0.132708f,  -0.468716f, 0.620680f,
    0.076890f,  0.829778f,  0.656897f,  0.598841f,  0.944444f,  0.000000f};
constexpr int NumShadowSamples = 17;
constexpr float PointLightBlurAngle = 3.0f * PI / 180.0f;
constexpr float StaticShadowBlurAngle = 10.0f * PI / 180.0f;
constexpr float ShadowNearZ = 0.1f;
constexpr float StaticShadowNearZ = 0.01f;
constexpr int MinSpotShadowMapSize = 16;
constexpr float ShadowFadeStart = 0.8f;
constexpr float DirectionalShadowNearExtension = 20.0f;
constexpr float CascadeSplitWeight = 0.3f;
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
    return CommonResourcesGL::DefaultMaterial;
}
static TextureGL* textureOf(RenderableTexture* texture)
{
    return ((RenderableTextureGL*)texture)->getTexture();
}
// Registry indices, the sort keys of the state changes.
static unsigned int programIndex(ShaderProgramGL* program)
{
    return (unsigned int)program->getRegistryIndex();
}
static unsigned int materialIndex(const Material* material)
{
    return (unsigned int)material->getRegistryIndex();
}
// 0x004cdb90: how far the mesh has dissolved: the explicit amount, or the position in
// the dissolve distance range.
static float dissolveAmount(const MeshEntity& entity)
{
    float start = entity.getDissolveStart(), end = entity.getDissolveEnd();
    if (start == 0.0f && end == 0.0f)
        return entity.getDissolve();
    float d = entity.getDistance();
    if (end <= start)
    {
        if (d < end * end)
            return 1.0f;
        if (d > start * start)
            return entity.getDissolve();
    }
    else
    {
        if (d < start * start)
            return entity.getDissolve();
        if (d > end * end)
            return 1.0f;
    }
    float amount = (sqrtf(d) - start) / (end - start);
    return amount > entity.getDissolve() ? amount : entity.getDissolve();
}

// 0x004e90e0
LightPrePassRendererGL::LightPrePassRendererGL(RenderContextGL* context, int width, int height)
    : m_pContext(context), m_width(width), m_height(height), m_viewportX(0), m_viewportY(0),
      m_viewportWidth(0), m_viewportHeight(0), m_clearColor(0, 0, 0, 0), m_diffuseMapping(true),
      m_normalMapping(true), m_renderMeshes(true), m_renderShadows(true), m_textureFilter(0),
      m_shadowQuality(0), m_pParticleRenderer(0), m_pShadowVisitor(0), m_pBlur(0),
      m_depthStencilBuffer(0), m_pNormalBuffer(0), m_pGlossinessBuffer(0), m_pLightBuffer(0),
      m_pFrameBuffer(0), m_pDefaultShader(0), m_pDissolveShader(0), m_pAmbientLightProgram(0),
      m_pStencilProgram(0), m_pSpotLightStencilProgram(0), m_pBlurCubeMapProgram(0)
{
    memset(m_shadowMaps, 0, sizeof(m_shadowMaps));
    memset(m_shadowMapTemps, 0, sizeof(m_shadowMapTemps));
    memset(m_shadowCubeMaps, 0, sizeof(m_shadowCubeMaps));
    memset(m_shadowCubeMapTemps, 0, sizeof(m_shadowCubeMapTemps));
    memset(m_shadowDepthBuffers, 0, sizeof(m_shadowDepthBuffers));
    memset(m_pointLightPrograms, 0, sizeof(m_pointLightPrograms));
    memset(m_spotLightPrograms, 0, sizeof(m_spotLightPrograms));
    memset(m_directionalLightPrograms, 0, sizeof(m_directionalLightPrograms));
    m_projection.makeIdentity();
    m_pParticleRenderer = new ParticleSystemRendererGL(m_pContext);
    m_pShadowVisitor = new RenderVisitor;
    m_pBlur = new BlurGL(m_pContext);
    m_pDefaultShader = new RenderableShaderGL;
    m_pDefaultShader->initLightPrePassRendererShader(0, 0, 0, 0, 0);
    m_pDissolveShader = new RenderableShaderGL;
    m_pDissolveShader->initLightPrePassRendererShader(0, "shaders/gl/MeshGeometryDissolve.fsh",
                                                      "shaders/gl/MeshMaterialDissolve.fsh", 0, 0);
    setProgram(m_pAmbientLightProgram,
               new ShaderProgramGL("shaders/gl/AmbientLight.vsh", "shaders/gl/AmbientLight.fsh"));
    static constexpr const char* defShadow[] = {"CAST_SHADOW", 0};
    static constexpr const char* defSpecular[] = {"SPECULAR", 0};
    static constexpr const char* defSpecularShadow[] = {"SPECULAR", "CAST_SHADOW", 0};
    const char* const* defines[4] = {0, defShadow, defSpecular, defSpecularShadow};
    struct LightShader
    {
        const char* vsh;
        const char* fsh;
        ShaderProgramGL** programs;
    } lightShaders[3] = {
        {"shaders/gl/PointLight.vsh", "shaders/gl/PointLight.fsh", m_pointLightPrograms},
        {"shaders/gl/SpotLight.vsh", "shaders/gl/SpotLight.fsh", m_spotLightPrograms},
        {"shaders/gl/DirectionalLight.vsh", "shaders/gl/DirectionalLight.fsh",
         m_directionalLightPrograms}};
    for (int s = 0; s < 3; ++s)
    {
        GLuint vs =
            RenderContextGL::compileShaderFromFile(lightShaders[s].vsh, GL_VERTEX_SHADER, 0);
        for (int v = 0; v < 4; ++v)
        {
            GLuint fs = RenderContextGL::compileShaderFromFile(lightShaders[s].fsh,
                                                               GL_FRAGMENT_SHADER, defines[v]);
            setProgram(lightShaders[s].programs[v], new ShaderProgramGL(vs, fs));
            glDeleteShader(fs);
        }
        glDeleteShader(vs);
    }
    setProgram(m_pStencilProgram,
               new ShaderProgramGL("shaders/gl/Stencil.vsh", "shaders/gl/Stencil.fsh"));
    setProgram(m_pSpotLightStencilProgram,
               new ShaderProgramGL("shaders/gl/SpotLightStencil.vsh", "shaders/gl/Stencil.fsh"));
    setProgram(m_pBlurCubeMapProgram,
               new ShaderProgramGL("shaders/gl/BlurCubeMap.vsh", "shaders/gl/BlurCubeMap.fsh"));
    resizeRenderBuffers(width, height);
    // the directional light shadow map and the shadow cube maps for sizes 16..2048
    Texture2DGL *map, *temp;
    getShadowMap(1024, map, temp);
    getShadowDepthBuffer(1024);
    for (int size = 16; size <= 2048; size *= 2)
    {
        TextureCubeGL *cube, *cubeTemp;
        getShadowCubeMap(size, cube, cubeTemp);
    }
}
// 0x004e9be0
LightPrePassRendererGL::~LightPrePassRendererGL()
{
    for (int i = 0; i < NumShadowMapSizes; ++i)
    {
        delete m_shadowMaps[i];
        delete m_shadowMapTemps[i];
        delete m_shadowCubeMaps[i];
        delete m_shadowCubeMapTemps[i];
        if (m_shadowDepthBuffers[i])
            glDeleteRenderbuffers(1, &m_shadowDepthBuffers[i]);
    }
    delete m_pBlurCubeMapProgram;
    delete m_pSpotLightStencilProgram;
    delete m_pStencilProgram;
    for (int i = 0; i < 4; ++i)
    {
        delete m_directionalLightPrograms[i];
        delete m_spotLightPrograms[i];
        delete m_pointLightPrograms[i];
    }
    delete m_pAmbientLightProgram;
    delete m_pDissolveShader;
    delete m_pDefaultShader;
    delete m_pFrameBuffer;
    delete m_pLightBuffer;
    delete m_pNormalBuffer;
    delete m_pGlossinessBuffer;
    m_pGeometryBuffer.reset(0);
    glDeleteRenderbuffers(1, &m_depthStencilBuffer);
    delete m_pBlur;
    delete m_pShadowVisitor;
    delete m_pParticleRenderer;
}

// 0x004e5c70
void LightPrePassRendererGL::setViewport(int x, int y, int width, int height)
{
    m_viewportX = x;
    m_viewportY = y;
    m_viewportWidth = width;
    m_viewportHeight = height;
}
// 0x004e7d90: four half float buffers and a packed depth stencil buffer; the geometry
// buffer is a renderable texture so Lua can read it (Renderer.getGeometryBuffer).
void LightPrePassRendererGL::resizeRenderBuffers(int width, int height)
{
    if (m_depthStencilBuffer)
        glDeleteRenderbuffers(1, &m_depthStencilBuffer);
    glGenRenderbuffers(1, &m_depthStencilBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthStencilBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    if (!m_pGeometryBuffer)
        m_pGeometryBuffer.reset(new RenderableTextureGL);
    m_pGeometryBuffer->setTexture(new Texture2DGL(width, height, 1, GL_RGBA16F, GL_NEAREST,
                                                  GL_NEAREST, GL_CLAMP_TO_EDGE, GL_RGBA));
    setTexture2D(m_pGlossinessBuffer, new Texture2DGL(width, height, 1, GL_RGBA16F, GL_NEAREST,
                                                      GL_NEAREST, GL_CLAMP_TO_EDGE, GL_RGBA));
    setTexture2D(m_pLightBuffer, new Texture2DGL(width, height, 1, GL_RGBA16F, GL_NEAREST,
                                                 GL_NEAREST, GL_CLAMP_TO_EDGE, GL_RGBA));
    setTexture2D(m_pFrameBuffer, new Texture2DGL(width, height, 1, GL_RGBA16F, GL_NEAREST,
                                                 GL_NEAREST, GL_CLAMP_TO_EDGE, GL_RGBA));
    m_width = width;
    m_height = height;
}
// Viewport in GL window coordinates (y up).
void LightPrePassRendererGL::setViewportGL()
{
    glViewport(m_viewportX, m_height - m_viewportY - m_viewportHeight, m_viewportWidth,
               m_viewportHeight);
    checkGLErrors("glViewport");
}

// ---- shadow map pools ------------------------------------------------------------

int LightPrePassRendererGL::sizeIndex(int size)
{
    int index = 0;
    if (size != 1)
        while (index < 11 && (1 << ++index) != size)
            ;
    return index;
}
// 0x004e7740
void LightPrePassRendererGL::getShadowMap(int size, Texture2DGL*& map, Texture2DGL*& temp)
{
    int index = sizeIndex(size);
    if (!m_shadowMaps[index])
    {
        debugPrint("alloc shadow map %d (%d)\n", size, index);
        m_shadowMaps[index] = new Texture2DGL(size, size, 1, GL_RG16, GL_NEAREST, GL_NEAREST,
                                              GL_CLAMP_TO_EDGE, GL_RGBA);
    }
    if (!m_shadowMapTemps[index])
    {
        debugPrint("alloc temp shadow map %d (%d)\n", size, index);
        m_shadowMapTemps[index] = new Texture2DGL(size, size, 1, GL_RG16, GL_NEAREST, GL_NEAREST,
                                                  GL_CLAMP_TO_EDGE, GL_RGBA);
    }
    map = m_shadowMaps[index];
    temp = m_shadowMapTemps[index];
}
// 0x004e7820
void LightPrePassRendererGL::getShadowCubeMap(int size, TextureCubeGL*& map, TextureCubeGL*& temp)
{
    int index = sizeIndex(size);
    if (!m_shadowCubeMaps[index])
    {
        debugPrint("alloc shadow cube map %d (%d)\n", size, index);
        m_shadowCubeMaps[index] = new TextureCubeGL(size, 1, GL_RG16);
    }
    if (!m_shadowCubeMapTemps[index])
    {
        debugPrint("alloc temp shadow cube map %d (%d)\n", size, index);
        m_shadowCubeMapTemps[index] = new TextureCubeGL(size, 1, GL_RG16);
    }
    map = m_shadowCubeMaps[index];
    temp = m_shadowCubeMapTemps[index];
}
// 0x004e6720
GLuint LightPrePassRendererGL::getShadowDepthBuffer(int size)
{
    int index = sizeIndex(size);
    if (!m_shadowDepthBuffers[index])
    {
        debugPrint("alloc shadow depth buffer %d (%d)\n", size, index);
        glGenRenderbuffers(1, &m_shadowDepthBuffers[index]);
        glBindRenderbuffer(GL_RENDERBUFFER, m_shadowDepthBuffers[index]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, size, size);
    }
    return m_shadowDepthBuffers[index];
}

// ---- passes ----------------------------------------------------------------------

// Oblique near plane clipping (Lengyel): the first user clip plane replaces the near
// plane of the (D3D style) projection.
static Matrix4x4 obliqueProjection(const Camera& camera)
{
    Matrix4x4 proj = camera.getProjectionMatrix();
    const Plane& worldPlane = camera.getUserClipPlane(0);
    // the plane in view space (the view is orthonormal, so the normal just rotates)
    const Matrix4x3& cameraToWorld = camera.getLocalToWorldMatrix();
    Vec3 n = camera.getWorldToLocalMatrix().rotation().transform(worldPlane.normal);
    float d = worldPlane.d - dot(worldPlane.normal, cameraToWorld.pos);
    Vec4 c(n.x, n.y, n.z, -d);
    float invLen = 1.0f / fabsf(c.z);
    c = c * invLen;
    if (c.z < 0.0f)
        c = c * -1.0f;
    Matrix4x4 invProj = camera.getInverseProjectionMatrix();
    Vec4 q =
        invProj.transform(Vec4(c.x < 0.0f ? -1.0f : 1.0f, c.y < 0.0f ? -1.0f : 1.0f, 1.0f, 1.0f));
    Vec4 m = c * (1.0f / dot(c, q));
    // third row (z output) of the column major matrix
    proj.m[2] = m.x;
    proj.m[6] = m.y;
    proj.m[10] = m.z;
    proj.m[14] = m.w;
    return proj;
}

// 0x004ea140: normals + depth and glossiness into two targets, opaque materials only,
// sorted by program and material.
void LightPrePassRendererGL::renderGeometryPass(const Camera& camera, const RenderVisitor& visitor)
{
    ProfileScope profile("_RenderGeometryPass");
    m_pContext->setRenderTarget((Texture2DGL*)textureOf(m_pGeometryBuffer.get()),
                                m_pGlossinessBuffer, m_depthStencilBuffer, m_depthStencilBuffer);
    setViewportGL();
    if (camera.getUserClipPlaneMask() == 1)
        m_projection = obliqueProjection(camera);
    else
        m_projection = camera.getProjectionMatrix();
    static constexpr GLenum buffers[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, buffers);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    m_pContext->setBlendMode(RenderContextGL::Blend_Opaque);
    glDepthFunc(GL_LEQUAL);
    glClearColor(0, 0, 0, camera.getFar());
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    Matrix4x4 proj = RenderContextGL::sm_d3dToGLProj * m_projection;

    static Array<DrawItem> items;
    items.clear();
    items.reserve(visitor.m_meshes.size());
    for (int e = 0; e < visitor.m_meshes.size(); ++e)
    {
        const MeshEntity& entity = *visitor.m_meshes[e];
        RenderableMeshGL* mesh = (RenderableMeshGL*)entity.getMesh();
        float dissolve = dissolveAmount(entity);
        bool skinned = entity.isSkinned() && mesh->isSkinned();
        bool tangents = mesh->hasTangents() && m_normalMapping;
        for (int s = 0; s < mesh->getNumSegments(); ++s)
        {
            const Material* material = segmentMaterial(entity, s);
            if (material->getBlendMode() != Material::Opaque)
                continue;
            RenderableShaderGL* shader = (RenderableShaderGL*)material->getShader();
            if (!shader)
                shader = m_pDefaultShader;
            if (dissolve > 0.0f)
                shader = m_pDissolveShader;
            int variant =
                (skinned ? RenderableShaderGL::Skinning : 0) |
                (tangents && material->getNormalMap() ? RenderableShaderGL::NormalMap : 0) |
                (material->getAlphaTest() ? RenderableShaderGL::AlphaTest : 0);
            DrawItem item;
            item.material = materialIndex(material);
            item.program = programIndex(shader->getProgram(variant));
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
        const MeshEntity& entity = *visitor.m_meshes[item.entity];
        RenderableMeshGL* mesh = (RenderableMeshGL*)entity.getMesh();
        float dissolve = dissolveAmount(entity);
        bool skinned = entity.isSkinned() && mesh->isSkinned();
        glBindVertexArray(mesh->getVertexArray());
        const Material* material = segmentMaterial(entity, item.segment);
        ShaderProgramGL* program = ShaderProgramGL::sm_programs[item.program];
        if (program != currentProgram)
        {
            m_pContext->useProgram(program);
            glUniformMatrix4fv(program->getUniform(ShaderProgramGL::U_proj), 1, GL_FALSE, proj.m);
            if (dissolve > 0.0f)
                m_pContext->setUniformTexture(ShaderProgramGL::U_dissolveMap,
                                              textureOf(CommonResourcesGL::NoiseMap), -1, -1, 2);
            ++g_renderStats.bindShader;
            currentProgram = program;
        }
        const Matrix4x3& localToWorld = entity.getNode()->getLocalToWorldMatrix();
        Matrix4x4 modelView(camera.getWorldToLocalMatrix() * localToWorld);
        glUniformMatrix4fv(program->getUniform(ShaderProgramGL::U_modelView), 1, GL_FALSE,
                           modelView.m);
        if (program->getUniform(ShaderProgramGL::U_model) >= 0)
        {
            Matrix4x4 model(localToWorld);
            glUniformMatrix4fv(program->getUniform(ShaderProgramGL::U_model), 1, GL_FALSE, model.m);
        }
        if (skinned)
            m_pContext->setSkinningMatrices(entity);
        Vec4 tso = mesh->getTexcoordScaleOffset();
        const Vec4& mtso = material->getTexcoordScaleOffset();
        glUniform4f(program->getUniform(ShaderProgramGL::U_texcoordScaleOffset), mtso.x * tso.x,
                    mtso.y * tso.y, mtso.z + tso.z, mtso.w + tso.w);
        if (dissolve > 0.0f)
            glUniform1f(program->getUniform(ShaderProgramGL::U_dissolve), dissolve);
        if (material != currentMaterial)
        {
            if (material->getDoubleSided())
                glDisable(GL_CULL_FACE);
            else
                glEnable(GL_CULL_FACE);
            glUniform1f(program->getUniform(ShaderProgramGL::U_glossiness),
                        material->getGlossiness());
            RenderableTexture* diffuse = material->getDiffuseMap();
            if (!diffuse || !m_diffuseMapping)
                diffuse = CommonResourcesGL::GrayMap;
            RenderableTexture* normal = material->getNormalMap();
            if (!normal || !m_normalMapping)
                normal = CommonResourcesGL::DefaultNormalMap;
            int address = material->getTextureAddressMode();
            m_pContext->setUniformTexture(ShaderProgramGL::U_diffuseMap, textureOf(diffuse), -1,
                                          address, 0);
            m_pContext->setUniformTexture(ShaderProgramGL::U_normalMap, textureOf(normal), -1,
                                          address, 1);
            if (material->getParamCount() > 0)
                m_pContext->setShaderParams(*material, 3);
            ++g_renderStats.bindMaterial;
            currentMaterial = material;
        }
        const RenderableMeshGL::Segment& segment = mesh->getSegment(item.segment);
        glDrawElements(GL_TRIANGLES, segment.primitiveCount * 3,
                       mesh->getIndexSize() == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
                       (const void*)(intptr_t)(segment.firstIndex * mesh->getIndexSize()));
        ++g_renderStats.drawSegments;
        g_renderStats.renderTriangles += segment.primitiveCount;
    }
    glDrawBuffers(1, buffers);
}

// 0x004ea9d0: sorted draw of the given meshes: pass 1 the lit materials with ambient
// occlusion, pass 2 the ones without, pass 3 the blended materials (unlit shaders).
void LightPrePassRendererGL::renderMeshes(const Camera& camera, MeshEntity* const* meshes,
                                          int count, int pass)
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
        float dissolve = dissolveAmount(entity);
        bool skinned = entity.isSkinned() && mesh->isSkinned();
        for (int s = 0; s < mesh->getNumSegments(); ++s)
        {
            const Material* material = segmentMaterial(entity, s);
            bool opaque = material->getBlendMode() == Material::Opaque;
            bool lit, unlit, alphaTest = false;
            if (pass == MaterialPass_Transparent)
            {
                if (opaque)
                    continue;
                lit = false;
                unlit = true;
            }
            else
            {
                if (!opaque)
                    continue;
                bool ao = material->getAmbientOcclusion();
                if ((pass == MaterialPass_AmbientOcclusion) != ao)
                    continue;
                alphaTest = material->getAlphaTest();
                unlit = !material->getLighting();
                lit = !unlit;
            }
            RenderableShaderGL* shader = (RenderableShaderGL*)material->getShader();
            if (!shader)
                shader = m_pDefaultShader;
            if (dissolve > 0.0f)
                shader = m_pDissolveShader;
            int variant = (skinned ? RenderableShaderGL::Skinning : 0) |
                          (alphaTest ? RenderableShaderGL::AlphaTest : 0) |
                          (lit ? RenderableShaderGL::MaterialPass : 0) |
                          (unlit ? RenderableShaderGL::UnlitPass : 0);
            DrawItem item;
            item.material = materialIndex(material);
            item.program = programIndex(shader->getProgram(variant));
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
        float dissolve = dissolveAmount(entity);
        bool skinned = entity.isSkinned() && mesh->isSkinned();
        glBindVertexArray(mesh->getVertexArray());
        const Material* material = segmentMaterial(entity, item.segment);
        ShaderProgramGL* program = ShaderProgramGL::sm_programs[item.program];
        if (program != currentProgram)
        {
            m_pContext->useProgram(program);
            glUniformMatrix4fv(program->getUniform(ShaderProgramGL::U_proj), 1, GL_FALSE, proj.m);
            glUniform2f(program->getUniform(ShaderProgramGL::U_invScreenSize), 1.0f / m_width,
                        1.0f / m_height);
            m_pContext->setUniformTexture(ShaderProgramGL::U_dissolveMap,
                                          textureOf(CommonResourcesGL::NoiseMap), -1, -1, 4);
            m_pContext->setUniformTexture(ShaderProgramGL::U_lightBuffer, m_pLightBuffer, -1, -1,
                                          5);
            m_pContext->setUniformTexture(ShaderProgramGL::U_geometryBuffer,
                                          textureOf(m_pGeometryBuffer.get()), -1, -1, 6);
            ++g_renderStats.bindShader;
            currentProgram = program;
        }
        const Matrix4x3& localToWorld = entity.getNode()->getLocalToWorldMatrix();
        Matrix4x4 modelView(camera.getWorldToLocalMatrix() * localToWorld);
        glUniformMatrix4fv(program->getUniform(ShaderProgramGL::U_modelView), 1, GL_FALSE,
                           modelView.m);
        if (program->getUniform(ShaderProgramGL::U_model) >= 0)
        {
            Matrix4x4 model(localToWorld);
            glUniformMatrix4fv(program->getUniform(ShaderProgramGL::U_model), 1, GL_FALSE, model.m);
        }
        if (skinned)
            m_pContext->setSkinningMatrices(entity);
        const Vec3& emissive = entity.getEmissiveColor();
        glUniform3f(program->getUniform(ShaderProgramGL::U_emissiveColor), emissive.x, emissive.y,
                    emissive.z);
        Vec4 tso = mesh->getTexcoordScaleOffset();
        const Vec4& mtso = material->getTexcoordScaleOffset();
        glUniform4f(program->getUniform(ShaderProgramGL::U_texcoordScaleOffset), mtso.x * tso.x,
                    mtso.y * tso.y, mtso.z + tso.z, mtso.w + tso.w);
        if (dissolve > 0.0f)
            glUniform1f(program->getUniform(ShaderProgramGL::U_dissolve), dissolve);
        if (material != currentMaterial)
        {
            if (material->getDoubleSided())
                glDisable(GL_CULL_FACE);
            else
                glEnable(GL_CULL_FACE);
            m_pContext->setBlendMode(material->getBlendMode());
            glUniform1f(program->getUniform(ShaderProgramGL::U_glossiness),
                        material->getGlossiness());
            RenderableTexture* diffuse = material->getDiffuseMap();
            if (!diffuse || !m_diffuseMapping)
                diffuse = CommonResourcesGL::GrayMap;
            RenderableTexture* specular = material->getSpecularMap();
            if (!specular || !m_diffuseMapping)
                specular = CommonResourcesGL::WhiteMap;
            RenderableTexture* normal = material->getNormalMap();
            if (!normal || !m_normalMapping)
                normal = CommonResourcesGL::DefaultNormalMap;
            RenderableTexture* emissiveMap = material->getEmissiveMap();
            if (!emissiveMap)
                emissiveMap = CommonResourcesGL::BlackMap;
            int address = material->getTextureAddressMode();
            m_pContext->setUniformTexture(ShaderProgramGL::U_diffuseMap, textureOf(diffuse), -1,
                                          address, 0);
            m_pContext->setUniformTexture(ShaderProgramGL::U_specularMap, textureOf(specular), -1,
                                          address, 1);
            m_pContext->setUniformTexture(ShaderProgramGL::U_normalMap, textureOf(normal), -1,
                                          address, 2);
            m_pContext->setUniformTexture(ShaderProgramGL::U_emissiveMap, textureOf(emissiveMap),
                                          -1, address, 3);
            if (material->getParamCount() > 0)
                m_pContext->setShaderParams(*material, 7);
            ++g_renderStats.bindMaterial;
            currentMaterial = material;
        }
        const RenderableMeshGL::Segment& segment = mesh->getSegment(item.segment);
        glDrawElements(GL_TRIANGLES, segment.primitiveCount * 3,
                       mesh->getIndexSize() == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
                       (const void*)(intptr_t)(segment.firstIndex * mesh->getIndexSize()));
        ++g_renderStats.drawSegments;
        g_renderStats.renderTriangles += segment.primitiveCount;
    }
}

// 0x004eb990: shades the opaque meshes with the light buffer into the frame buffer.
void LightPrePassRendererGL::renderMaterialPass(const Camera& camera, const RenderVisitor& visitor,
                                                int pass)
{
    ProfileScope profile("_RenderMaterialPass");
    m_pContext->setRenderTarget(m_pFrameBuffer, 0, m_depthStencilBuffer, m_depthStencilBuffer);
    setViewportGL();
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    if (pass == MaterialPass_AmbientOcclusion)
    {
        glClearColor(m_clearColor.r / 255.0f, m_clearColor.g / 255.0f, m_clearColor.b / 255.0f,
                     m_clearColor.a / 255.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    renderMeshes(camera, visitor.m_meshes.data(), visitor.m_meshes.size(), pass);
}

// 0x004ebad0: blended meshes of the given pass and the particle systems (last pass),
// sorted back to front.
void LightPrePassRendererGL::renderTransparentPass(const Camera& camera,
                                                   const RenderVisitor& visitor, int pass)
{
    ProfileScope profile("_RenderTransparentPass");
    m_pContext->setRenderTarget(m_pFrameBuffer, 0, m_depthStencilBuffer, m_depthStencilBuffer);
    setViewportGL();
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
            int entityPass = TransparentPass_Last;
            if (entity->getRenderHack() & RenderHack_FirstPass)
                entityPass = TransparentPass_First;
            else if (entity->getRenderHack() & RenderHack_SecondPass)
                entityPass = TransparentPass_Second;
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
    if (pass == TransparentPass_Last)
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
    for (int i = 0; i < transparents.size(); ++i)
    {
        RenderEntity* entity = transparents[i].entity;
        if (entity->getEntityType() == RenderEntity::MeshEntityType)
        {
            MeshEntity* mesh = (MeshEntity*)entity;
            if (mesh->getRenderHack() & RenderHack_DepthWrite)
                glDepthMask(GL_TRUE);
            if (mesh->getRenderHack() & RenderHack_NoDepthTest)
                glDisable(GL_DEPTH_TEST);
            renderMeshes(camera, &mesh, 1, MaterialPass_Transparent);
            if (mesh->getRenderHack() & RenderHack_DepthWrite)
                glDepthMask(GL_FALSE);
            if (mesh->getRenderHack() & RenderHack_NoDepthTest)
                glEnable(GL_DEPTH_TEST);
        }
        else
        {
            m_pParticleRenderer->renderParticleSystem(camera, *(ParticleEntity*)entity,
                                                      m_diffuseMapping, m_projection);
        }
    }
}

// 0x004ed100: accumulates all lights into the light buffer; shadow maps are rendered on
// demand, the directional light in four cascades.
void LightPrePassRendererGL::renderLightPass(const Camera& camera, const RenderVisitor& visitor)
{
    ProfileScope profile("_RenderLightPass");
    m_pContext->setRenderTarget(m_pLightBuffer, 0, m_depthStencilBuffer, m_depthStencilBuffer);
    setViewportGL();
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    Matrix4x4 identity;
    identity.makeIdentity();
    for (int i = 0; i < visitor.m_lights.size(); ++i)
    {
        LightEntity& light = *visitor.m_lights[i];
        bool shadows = light.getCastShadow() && m_renderShadows;
        switch (light.getLightType())
        {
        case LightEntity::Ambient:
            renderAmbientLight(camera, light);
            break;
        case LightEntity::Directional:
            if (!shadows)
            {
                renderDirectionalLight(camera, light, 0, identity, 0, 0.0f, 0.0f);
                break;
            }
            else
            {
                // practical split scheme between the near plane and the shadow distance
                float nearZ = camera.getNear();
                float farZ = light.getMaxShadowDistance();
                if (camera.getFar() < farZ)
                    farZ = camera.getFar();
                float splits[NumCascades + 1];
                for (int c = 0; c <= NumCascades; ++c)
                {
                    float p = (float)c * 0.25f;
                    splits[c] = CascadeSplitWeight * (nearZ + (farZ - nearZ) * p) +
                                (1.0f - CascadeSplitWeight) * nearZ * powf(farZ / nearZ, p);
                }
                for (int c = 0; c < NumCascades; ++c)
                {
                    Texture2DGL *map, *temp;
                    getShadowMap(light.getShadowMapSize(), map, temp);
                    GLuint depth = getShadowDepthBuffer(light.getShadowMapSize());
                    Matrix4x4 shadowViewProj;
                    renderDirectionalLightShadowMap(light, camera, map, depth, splits[c],
                                                    splits[c + 1], shadowViewProj);
                    m_pContext->setRenderTarget(m_pLightBuffer, 0, m_depthStencilBuffer,
                                                m_depthStencilBuffer);
                    setViewportGL();
                    renderDirectionalLight(camera, light, map, shadowViewProj, c, splits[c],
                                           splits[c + 1]);
                }
            }
            break;
        case LightEntity::Point:
        {
            unsigned int faceMask = light.getVisibleFaces();
            if (faceMask == 0)
                break;
            if (!shadows)
            {
                renderPointLight(camera, light, 0);
            }
            else if (!light.getStaticShadowMap())
            {
                TextureCubeGL *cube, *temp;
                getShadowCubeMap(light.getShadowMapSize(), cube, temp);
                GLuint depth = getShadowDepthBuffer(light.getShadowMapSize());
                if (m_shadowQuality < 1)
                {
                    renderPointLightShadowMap(light, camera, cube, depth, false, faceMask);
                }
                else
                {
                    renderPointLightShadowMap(light, camera, temp, depth, false, faceMask);
                    blurCubeMapGPU(temp, cube, PointLightBlurAngle, faceMask);
                }
                m_pContext->setRenderTarget(m_pLightBuffer, 0, m_depthStencilBuffer,
                                            m_depthStencilBuffer);
                setViewportGL();
                renderPointLight(camera, light, cube);
            }
            else
            {
                renderPointLight(camera, light,
                                 (TextureCubeGL*)textureOf(light.getStaticShadowMap()));
            }
            break;
        }
        case LightEntity::Spot:
        {
            Texture2DGL* shadowMap = 0;
            if (shadows)
            {
                if (!light.getStaticShadowMap())
                {
                    Texture2DGL* temp;
                    getShadowMap(light.getShadowMapSize(), shadowMap, temp);
                    GLuint depth = getShadowDepthBuffer(light.getShadowMapSize());
                    renderSpotLightShadowMap(light, camera, shadowMap, depth, false);
                    if (m_shadowQuality >= 1)
                        m_pBlur->blur(shadowMap, temp);
                    m_pContext->setRenderTarget(m_pLightBuffer, 0, m_depthStencilBuffer,
                                                m_depthStencilBuffer);
                    setViewportGL();
                }
                else
                {
                    shadowMap = (Texture2DGL*)textureOf(light.getStaticShadowMap());
                }
            }
            renderSpotLight(camera, light, shadowMap);
            break;
        }
        }
    }
}

// 0x004e5ca0: hemispherical ambient light: lightColor at the zenith, lightColor2 at the
// horizon, lightColor3 below.
void LightPrePassRendererGL::renderAmbientLight(const Camera& camera, const LightEntity& light)
{
    m_pContext->useProgram(m_pAmbientLightProgram);
    GLuint program = m_pAmbientLightProgram->getProgram();
    glUniform2f(m_pAmbientLightProgram->getUniform(ShaderProgramGL::U_invScreenSize),
                1.0f / m_width, 1.0f / m_height);
    const Vec3& c = light.getLightColor();
    glUniform3f(m_pAmbientLightProgram->getUniform(ShaderProgramGL::U_lightColor), c.x, c.y, c.z);
    const Vec3& c2 = light.getLightColor2();
    glUniform3f(glGetUniformLocation(program, "g_lightColor2"), c2.x, c2.y, c2.z);
    const Vec3& c3 = light.getLightColor3();
    glUniform3f(glGetUniformLocation(program, "g_lightColor3"), c3.x, c3.y, c3.z);
    // world up in view space
    const Matrix4x3& worldToView = camera.getWorldToLocalMatrix();
    glUniform3f(glGetUniformLocation(program, "g_zenith"), worldToView.y.x, worldToView.y.y,
                worldToView.y.z);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);
    m_pContext->setBlendMode(RenderContextGL::Blend_Additive);
    m_pContext->drawRect();
}

// Clip space depth (D3D convention, 0..1) of a view space z.
static float projectedDepth(const Camera& camera, float z)
{
    Vec4 p = camera.getProjectionMatrix().transform(Vec4(0.0f, 0.0f, z, 1.0f));
    return p.z / p.w;
}

// 0x004e6990
void LightPrePassRendererGL::renderDirectionalLight(const Camera& camera, const LightEntity& light,
                                                    Texture2DGL* shadowMap,
                                                    const Matrix4x4& shadowViewProj, int cascade,
                                                    float cascadeStart, float cascadeEnd)
{
    if (shadowMap && cascade < NumCascades - 1)
    {
        // mark everything beyond the cascade end in the stencil buffer
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        m_pContext->setBlendMode(RenderContextGL::Blend_Opaque);
        glEnable(GL_STENCIL_TEST);
        glDisable(GL_CULL_FACE);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glStencilFunc(GL_ALWAYS, 0, 0xffffffff);
        glStencilOp(GL_KEEP, GL_KEEP, GL_INCR_WRAP);
        m_pContext->useProgram(m_pStencilProgram);
        Matrix4x4 depth;
        depth.makeIdentity();
        depth.m[14] = projectedDepth(camera, cascadeEnd);
        Matrix4x4 mvp = RenderContextGL::sm_d3dToGLProj * depth;
        glUniformMatrix4fv(
            glGetUniformLocation(m_pStencilProgram->getProgram(), "g_viewProjMatrix"), 1, GL_FALSE,
            mvp.m);
        m_pContext->drawRect();
    }
    ShaderProgramGL* prog =
        m_directionalLightPrograms[(shadowMap ? 1 : 0) | (light.getSpecular() ? 2 : 0)];
    m_pContext->useProgram(prog);
    GLuint program = prog->getProgram();
    m_pContext->setUniformTexture(ShaderProgramGL::U_geometryBuffer,
                                  textureOf(m_pGeometryBuffer.get()), -1, -1, 0);
    m_pContext->setUniformTexture("g_glossinessBuffer", m_pGlossinessBuffer, -1, -1, 1);
    glUniform2f(prog->getUniform(ShaderProgramGL::U_invScreenSize),
                1.0f / m_pGeometryBuffer->getWidth(), 1.0f / m_pGeometryBuffer->getHeight());
    glUniform1f(glGetUniformLocation(program, "g_invNear"), 1.0f / camera.getNear());
    glUniformMatrix4fv(glGetUniformLocation(program, "g_invProjectionMatrix"), 1, GL_FALSE,
                       camera.getInverseProjectionMatrix().m);
    // light z axis in view space
    Vec3 dir = camera.getWorldToLocalMatrix().rotation().transform(
        light.getNode()->getLocalToWorldMatrix().z);
    glUniform3f(glGetUniformLocation(program, "g_lightDirection"), dir.x, dir.y, dir.z);
    const Vec3& c = light.getLightColor();
    glUniform3f(prog->getUniform(ShaderProgramGL::U_lightColor), c.x, c.y, c.z);
    if (shadowMap)
    {
        // view space to shadow map texture space
        Matrix4x4 shadowProj = shadowViewProj * Matrix4x4(camera.getLocalToWorldMatrix());
        Matrix4x4 bias;
        bias.makeIdentity();
        bias.m[0] = 0.5f;
        bias.m[5] = -0.5f;
        bias.m[12] = 0.5f / shadowMap->getWidth() + 0.5f;
        bias.m[13] = 0.5f / shadowMap->getHeight() + 0.5f;
        shadowProj = bias * shadowProj;
        m_pContext->setUniformTexture("g_shadowMap", shadowMap, Material::Nearest, Material::Clamp,
                                      2);
        glUniformMatrix4fv(glGetUniformLocation(program, "g_shadowProjMatrix"), 1, GL_FALSE,
                           shadowProj.m);
        m_pContext->setUniformTexture("g_rotTex", textureOf(CommonResourcesGL::RotMap),
                                      Material::Nearest, Material::Wrap, 3);
        glUniform2fv(glGetUniformLocation(program, "g_samples"), NumShadowSamples, ShadowSamples);
        // the shadow fades out over the last fifth of the shadow distance
        float farZ = light.getMaxShadowDistance();
        if (camera.getFar() < farZ)
            farZ = camera.getFar();
        float fadeStart = farZ * ShadowFadeStart;
        glUniform2f(glGetUniformLocation(program, "g_shadowFade"), 1.0f / (farZ - fadeStart),
                    -fadeStart / (farZ - fadeStart));
        // the vertex shader writes it into gl_Position, so the projected depth goes
        // through the D3D to GL depth range conversion
        float startDepth = 0.0f;
        if (cascade > 0)
        {
            Vec4 clip = RenderContextGL::sm_d3dToGLProj.transform(
                Vec4(0.0f, 0.0f, projectedDepth(camera, cascadeStart), 1.0f));
            startDepth = clip.z / clip.w;
        }
        glUniform1f(glGetUniformLocation(program, "g_cascadeStartZ"), startDepth);
    }
    else
    {
        glUniform1f(glGetUniformLocation(program, "g_cascadeStartZ"), 0.0f);
    }
    if (shadowMap)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);
    m_pContext->setBlendMode(RenderContextGL::Blend_Additive);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if (shadowMap)
    {
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_EQUAL, 0, 0xffffffff);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    }
    m_pContext->drawRect();
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);
    glDisable(GL_STENCIL_TEST);
}

// 0x004e72b0: scissor rectangle of a sphere in view space (Lengyel's method).
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
    int height = ymax - ymin > 0 ? ymax - ymin : 0;
    int width = xmax - xmin > 0 ? xmax - xmin : 0;
    int area = height * width;
    if (area <= 0 || xmin >= viewportWidth || ymin >= viewportHeight)
        return 0;
    if (viewportWidth * viewportHeight - area == 0)
    {
        glDisable(GL_SCISSOR_TEST);
        return viewportWidth * viewportHeight;
    }
    int originX = m_viewportX, originY = m_height - viewportHeight - m_viewportY;
    glEnable(GL_SCISSOR_TEST);
    glScissor(xmin + originX, ymin + originY, xmax - xmin, ymax - ymin);
    checkGLErrors("glScissor");
    return area;
}

// 0x004e6320: marks the pixels inside the light sphere in the stencil buffer; the
// winding is swapped for mirrored cameras.
void LightPrePassRendererGL::renderPointLightStencil(const Camera& camera, const LightEntity& light)
{
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    m_pContext->setBlendMode(RenderContextGL::Blend_Opaque);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 0, 0xffffffff);
    if (!camera.getInverseCulling())
    {
        glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP, GL_KEEP);
        glStencilOpSeparate(GL_BACK, GL_KEEP, GL_INCR_WRAP, GL_KEEP);
    }
    else
    {
        glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_INCR_WRAP, GL_KEEP);
        glStencilOpSeparate(GL_BACK, GL_KEEP, GL_DECR_WRAP, GL_KEEP);
    }
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
    glUniformMatrix4fv(glGetUniformLocation(m_pStencilProgram->getProgram(), "g_viewProjMatrix"), 1,
                       GL_FALSE, mvp.m);
    m_pContext->drawMesh(*CommonResourcesGL::SphereMesh);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}
// 0x004e6520: same with the cone of a spot light.
void LightPrePassRendererGL::renderSpotLightStencil(const Camera& camera, const LightEntity& light)
{
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    m_pContext->setBlendMode(RenderContextGL::Blend_Opaque);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 0, 0xffffffff);
    if (!camera.getInverseCulling())
    {
        glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP, GL_KEEP);
        glStencilOpSeparate(GL_BACK, GL_KEEP, GL_INCR_WRAP, GL_KEEP);
    }
    else
    {
        glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_INCR_WRAP, GL_KEEP);
        glStencilOpSeparate(GL_BACK, GL_KEEP, GL_DECR_WRAP, GL_KEEP);
    }
    m_pContext->useProgram(m_pSpotLightStencilProgram);
    GLuint program = m_pSpotLightStencilProgram->getProgram();
    // the unit cone is stretched to the range and the spot angle
    float range = light.getLightRange();
    float halfAngle = light.getSpotAngle() * 0.5f;
    float depth = range / std::cos(halfAngle);
    float radius = std::tan(halfAngle) * range;
    glUniform2f(glGetUniformLocation(program, "g_spotlightParams"),
                (radius / std::sin(halfAngle)) / depth, depth);
    Matrix4x4 mvp =
        RenderContextGL::sm_d3dToGLProj *
        (camera.getViewProjectionMatrix() * Matrix4x4(light.getNode()->getLocalToWorldMatrix()));
    glUniformMatrix4fv(glGetUniformLocation(program, "g_viewProjMatrix"), 1, GL_FALSE, mvp.m);
    m_pContext->drawMesh(*CommonResourcesGL::ConeMesh);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

// 0x004e7fb0
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
    ShaderProgramGL* prog =
        m_pointLightPrograms[(shadowMap ? 1 : 0) | (light.getSpecular() ? 2 : 0)];
    m_pContext->useProgram(prog);
    GLuint program = prog->getProgram();
    m_pContext->setUniformTexture(ShaderProgramGL::U_geometryBuffer,
                                  textureOf(m_pGeometryBuffer.get()), -1, -1, 0);
    m_pContext->setUniformTexture("g_glossinessBuffer", m_pGlossinessBuffer, -1, -1, 1);
    glUniform2f(prog->getUniform(ShaderProgramGL::U_invScreenSize),
                1.0f / m_pLightBuffer->getWidth(), 1.0f / m_pLightBuffer->getHeight());
    glUniform1f(glGetUniformLocation(program, "g_invNear"), 1.0f / camera.getNear());
    glUniformMatrix4fv(glGetUniformLocation(program, "g_invProjectionMatrix"), 1, GL_FALSE,
                       camera.getInverseProjectionMatrix().m);
    glUniform3f(glGetUniformLocation(program, "g_lightPosition"), viewPos.x, viewPos.y, viewPos.z);
    const Vec3& c = light.getLightColor();
    glUniform3f(prog->getUniform(ShaderProgramGL::U_lightColor), c.x, c.y, c.z);
    glUniform1f(prog->getUniform(ShaderProgramGL::U_invLightRange),
                range > 0.0f ? 1.0f / range : 0.0f);
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
        m_pContext->setUniformTexture("g_shadowCubeMap", shadowMap, -1, -1, 2);
        Matrix4x4 shadowProj(m);
        glUniformMatrix4fv(glGetUniformLocation(program, "g_shadowProjMatrix"), 1, GL_FALSE,
                           shadowProj.m);
    }
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);
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

// 0x004e8440
void LightPrePassRendererGL::renderSpotLight(const Camera& camera, const LightEntity& light,
                                             Texture2DGL* shadowMap)
{
    float range = light.getLightRange();
    const Matrix4x3& worldToView = camera.getWorldToLocalMatrix();
    const Matrix4x3& lightToWorld = light.getNode()->getLocalToWorldMatrix();
    Vec3 viewPos = worldToView.transformPoint(lightToWorld.pos);
    if (!setupPointLightScissorRect(camera, viewPos, range))
        return;
    float halfAngle = light.getSpotAngle() * 0.5f;
    float depth = range / std::cos(light.getSpotAngle() * 0.25f);
    bool useStencil = viewPos.z + depth < camera.getFar();
    if (useStencil)
        renderSpotLightStencil(camera, light);
    ShaderProgramGL* prog =
        m_spotLightPrograms[(shadowMap ? 1 : 0) | (light.getSpecular() ? 2 : 0)];
    m_pContext->useProgram(prog);
    GLuint program = prog->getProgram();
    m_pContext->setUniformTexture(ShaderProgramGL::U_geometryBuffer,
                                  textureOf(m_pGeometryBuffer.get()), -1, -1, 0);
    m_pContext->setUniformTexture("g_glossinessBuffer", m_pGlossinessBuffer, -1, -1, 1);
    glUniform2f(prog->getUniform(ShaderProgramGL::U_invScreenSize),
                1.0f / m_pGeometryBuffer->getWidth(), 1.0f / m_pGeometryBuffer->getHeight());
    glUniform1f(glGetUniformLocation(program, "g_invNear"), 1.0f / camera.getNear());
    glUniformMatrix4fv(glGetUniformLocation(program, "g_invProjectionMatrix"), 1, GL_FALSE,
                       camera.getInverseProjectionMatrix().m);
    glUniform3f(glGetUniformLocation(program, "g_lightPosition"), viewPos.x, viewPos.y, viewPos.z);
    Vec3 dir = worldToView.rotation().transform(lightToWorld.z);
    glUniform3f(glGetUniformLocation(program, "g_lightDirection"), dir.x, dir.y, dir.z);
    const Vec3& c = light.getLightColor();
    glUniform3f(prog->getUniform(ShaderProgramGL::U_lightColor), c.x, c.y, c.z);
    // cosines of the full and the sharpened cone
    glUniform2f(glGetUniformLocation(program, "g_spotFalloff"), std::cos(halfAngle),
                std::cos(halfAngle * (1.0f - light.getSpotSharpness())));
    glUniform1f(prog->getUniform(ShaderProgramGL::U_invLightRange),
                range > 0.0f ? 1.0f / range : 0.0f);
    if (shadowMap)
    {
        // view space to the light's projected texture space, inside the region of the
        // shadow map that was rendered
        Matrix4x3 viewToLight =
            light.getNode()->getWorldToLocalMatrix() * camera.getLocalToWorldMatrix();
        Matrix4x4 lightProj;
        makePerspectiveProjectionMatrix(&lightProj, light.getSpotAngle(), 1.0f, ShadowNearZ, range);
        int mapSize = shadowMap->getWidth();
        int size = light.getShadowMapSize();
        if (size < MinSpotShadowMapSize)
            size = MinSpotShadowMapSize;
        if (size > mapSize)
            size = mapSize;
        float scale = (float)size / (float)mapSize * 0.5f;
        Matrix4x4 bias;
        bias.makeIdentity();
        bias.m[0] = scale;
        bias.m[5] = -scale;
        bias.m[12] = 0.5f / shadowMap->getWidth() + scale;
        bias.m[13] = 0.5f / shadowMap->getHeight() + scale;
        Matrix4x4 shadowProj = (bias * lightProj) * Matrix4x4(viewToLight);
        m_pContext->setUniformTexture("g_shadowMap", shadowMap, -1, -1, 2);
        glUniformMatrix4fv(glGetUniformLocation(program, "g_shadowProjMatrix"), 1, GL_FALSE,
                           shadowProj.m);
    }
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);
    m_pContext->setBlendMode(RenderContextGL::Blend_Additive);
    if (useStencil)
    {
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_EQUAL, 1, 0xff);
        glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);
        checkGLErrors("spotlight stencil");
    }
    m_pContext->drawRect();
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
}

// ---- shadow maps -----------------------------------------------------------------

// 0x004eb2a0: meshes whose materials all cast plain shadows are drawn whole, the rest
// segment by segment with the alpha tested diffuse map bound.
void LightPrePassRendererGL::renderShadowMeshes(const Array<MeshEntity*>& meshes,
                                                const Matrix4x4& view, const Matrix4x4& projection,
                                                float invLightRange, bool staticOnly, int variant)
{
    static Array<DrawItem> items;
    static Array<ShadowItem> wholeItems;
    items.clear();
    wholeItems.clear();
    for (int e = 0; e < meshes.size(); ++e)
    {
        const MeshEntity& entity = *meshes[e];
        if (staticOnly && !(entity.getFlags() & MeshEntity::StaticShadow))
            continue;
        RenderableMeshGL* mesh = (RenderableMeshGL*)entity.getMesh();
        bool skinned = entity.isSkinned() && mesh->isSkinned();
        int simple = 0;
        for (int s = 0; s < mesh->getNumSegments(); ++s)
        {
            const Material* material = segmentMaterial(entity, s);
            if (material->getBlendMode() == Material::Opaque && material->getCastShadow() &&
                !material->getDoubleSided() && !material->getAlphaTest())
                ++simple;
        }
        if (simple == mesh->getNumSegments())
        {
            ShadowItem item;
            item.program = programIndex(m_pDefaultShader->getProgram(
                variant | (skinned ? RenderableShaderGL::Skinning : 0)));
            item.entity = (unsigned int)e;
            wholeItems.push_back(item);
            continue;
        }
        for (int s = 0; s < mesh->getNumSegments(); ++s)
        {
            const Material* material = segmentMaterial(entity, s);
            if (material->getBlendMode() != Material::Opaque || !material->getCastShadow())
                continue;
            DrawItem item;
            item.material = materialIndex(material);
            item.program = programIndex(m_pDefaultShader->getProgram(
                variant | (skinned ? RenderableShaderGL::Skinning : 0) |
                (material->getAlphaTest() ? RenderableShaderGL::AlphaTest : 0)));
            item.entity = (unsigned int)e;
            item.segment = (unsigned int)s;
            items.push_back(item);
        }
    }
    std::sort(items.begin(), items.end());
    std::sort(wholeItems.begin(), wholeItems.end());

    ShaderProgramGL* currentProgram = 0;
    const Material* currentMaterial = 0;
    for (int i = 0; i < items.size(); ++i)
    {
        const DrawItem& item = items[i];
        const MeshEntity& entity = *meshes[item.entity];
        RenderableMeshGL* mesh = (RenderableMeshGL*)entity.getMesh();
        glBindVertexArray(mesh->getVertexArray());
        const Material* material = segmentMaterial(entity, item.segment);
        ShaderProgramGL* program = ShaderProgramGL::sm_programs[item.program];
        if (program != currentProgram)
        {
            m_pContext->useProgram(program);
            glUniform1f(program->getUniform(ShaderProgramGL::U_invLightRange), invLightRange);
            glUniformMatrix4fv(program->getUniform(ShaderProgramGL::U_proj), 1, GL_FALSE,
                               projection.m);
            ++g_renderStats.bindShader;
            currentProgram = program;
        }
        Matrix4x4 modelView = view * Matrix4x4(entity.getNode()->getLocalToWorldMatrix());
        glUniformMatrix4fv(program->getUniform(ShaderProgramGL::U_modelView), 1, GL_FALSE,
                           modelView.m);
        if (entity.isSkinned() && mesh->isSkinned())
            m_pContext->setSkinningMatrices(entity);
        if (material->getAlphaTest())
        {
            Vec4 tso = mesh->getTexcoordScaleOffset();
            const Vec4& mtso = material->getTexcoordScaleOffset();
            glUniform4f(program->getUniform(ShaderProgramGL::U_texcoordScaleOffset), mtso.x * tso.x,
                        mtso.y * tso.y, mtso.z + tso.z, mtso.w + tso.w);
        }
        if (material != currentMaterial)
        {
            if (material->getAlphaTest())
            {
                RenderableTexture* diffuse = material->getDiffuseMap();
                if (!diffuse)
                    diffuse = CommonResourcesGL::WhiteMap;
                m_pContext->setUniformTexture(ShaderProgramGL::U_diffuseMap, textureOf(diffuse), -1,
                                              material->getTextureAddressMode(), 0);
            }
            if (material->getDoubleSided())
                glDisable(GL_CULL_FACE);
            else
                glEnable(GL_CULL_FACE);
            ++g_renderStats.bindMaterial;
            currentMaterial = material;
        }
        const RenderableMeshGL::Segment& segment = mesh->getSegment(item.segment);
        glDrawElements(GL_TRIANGLES, segment.primitiveCount * 3,
                       mesh->getIndexSize() == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
                       (const void*)(intptr_t)(segment.firstIndex * mesh->getIndexSize()));
        ++g_renderStats.shadowSegments;
    }
    currentProgram = 0;
    glEnable(GL_CULL_FACE);
    for (int i = 0; i < wholeItems.size(); ++i)
    {
        const MeshEntity& entity = *meshes[wholeItems[i].entity];
        RenderableMeshGL* mesh = (RenderableMeshGL*)entity.getMesh();
        glBindVertexArray(mesh->getVertexArray());
        ShaderProgramGL* program = ShaderProgramGL::sm_programs[wholeItems[i].program];
        if (program != currentProgram)
        {
            m_pContext->useProgram(program);
            glUniform1f(program->getUniform(ShaderProgramGL::U_invLightRange), invLightRange);
            glUniformMatrix4fv(program->getUniform(ShaderProgramGL::U_proj), 1, GL_FALSE,
                               projection.m);
            ++g_renderStats.bindShader;
            currentProgram = program;
        }
        if (entity.isSkinned() && mesh->isSkinned())
            m_pContext->setSkinningMatrices(entity);
        Matrix4x4 modelView = view * Matrix4x4(entity.getNode()->getLocalToWorldMatrix());
        glUniformMatrix4fv(program->getUniform(ShaderProgramGL::U_modelView), 1, GL_FALSE,
                           modelView.m);
        glDrawElements(GL_TRIANGLES, mesh->getNumIndices(),
                       mesh->getIndexSize() == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, 0);
        ++g_renderStats.shadowSegments;
    }
}

// The six cube faces as camera rotations (the cube faces are stored upside down).
static const Matrix3x3 g_cubeFaceRotations[6] = {
    Matrix3x3(Vec3(0, 0, -1), Vec3(0, 1, 0), Vec3(1, 0, 0)),
    Matrix3x3(Vec3(0, 0, 1), Vec3(0, 1, 0), Vec3(-1, 0, 0)),
    Matrix3x3(Vec3(1, 0, 0), Vec3(0, 0, -1), Vec3(0, 1, 0)),
    Matrix3x3(Vec3(1, 0, 0), Vec3(0, 0, 1), Vec3(0, -1, 0)),
    Matrix3x3(Vec3(1, 0, 0), Vec3(0, 1, 0), Vec3(0, 0, 1)),
    Matrix3x3(Vec3(-1, 0, 0), Vec3(0, 1, 0), Vec3(0, 0, -1)),
};

// 0x004ebe70: depth of the cube faces in faceMask around a point light, each face
// reaching as far as the light's clip distance for it.
void LightPrePassRendererGL::renderPointLightShadowMap(const LightEntity& light,
                                                       const Camera& camera, TextureCubeGL* cube,
                                                       GLuint depthBuffer, bool staticOnly,
                                                       unsigned int faceMask)
{
    ProfileScope profile("_RenderPointLightShadowMap");
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    m_pContext->setBlendMode(RenderContextGL::Blend_Opaque);
    Matrix4x3 cameraMatrix;
    cameraMatrix.makeIdentity();
    cameraMatrix.pos = light.getNode()->getLocalToWorldMatrix().pos;
    float range = light.getLightRange();
    Matrix4x4 proj;
    makePerspectiveProjectionMatrix(&proj, HALF_PI, 1.0f, ShadowNearZ, range);
    Matrix4x4 flipY;
    flipY.m[5] = -1.0f;
    proj = RenderContextGL::sm_d3dToGLProj * (flipY * proj);
    glFrontFace(GL_CCW);
    for (int face = 0; face < 6; ++face)
    {
        m_pContext->setRenderTarget(cube, face, depthBuffer, 0);
        glViewport(0, 0, cube->getWidth(), cube->getHeight());
        checkGLErrors("glViewport");
        if (!(faceMask & (1u << face)))
        {
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            continue;
        }
        glClearColor(1, 1, 1, 1);
        glClearDepth(1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        cameraMatrix.rotation() = g_cubeFaceRotations[face];
        Matrix4x3 view = cameraMatrix;
        view.invertOrthonormal();
        float farZ = light.getClipDistance(face);
        if (farZ > range)
            farZ = range;
        Plane planes[6];
        computeFrustumPlanes(planes, HALF_PI, ShadowNearZ, farZ, cameraMatrix);
        m_pShadowVisitor->gatherShadowCasters(*light.getNode()->getScene(), camera, planes, 6);
        renderShadowMeshes(m_pShadowVisitor->m_meshes, Matrix4x4(view), proj, 1.0f / range,
                           staticOnly, RenderableShaderGL::ShadowPass);
    }
    glFrontFace(GL_CW);
    checkGLErrors("pointlight shadow");
}

// 0x004ec740
void LightPrePassRendererGL::renderSpotLightShadowMap(const LightEntity& light,
                                                      const Camera& camera, Texture2DGL* shadowMap,
                                                      GLuint depthBuffer, bool staticOnly)
{
    ProfileScope profile("_RenderSpotLightShadowMap");
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    m_pContext->setBlendMode(RenderContextGL::Blend_Opaque);
    float range = light.getLightRange();
    float farZ = light.getClipDistance(0);
    if (farZ > range)
        farZ = range;
    const Matrix4x3& lightToWorld = light.getNode()->getLocalToWorldMatrix();
    Plane planes[6];
    computeFrustumPlanes(planes, light.getSpotAngle(), 0.0f, farZ, lightToWorld);
    m_pShadowVisitor->gatherShadowCasters(*light.getNode()->getScene(), camera, planes, 6);
    Matrix4x4 proj;
    makePerspectiveProjectionMatrix(&proj, light.getSpotAngle(), 1.0f, ShadowNearZ, range);
    Matrix4x4 flipY;
    flipY.m[5] = -1.0f;
    proj = RenderContextGL::sm_d3dToGLProj * (flipY * proj);
    m_pContext->setRenderTarget(shadowMap, 0, depthBuffer, 0);
    glViewport(0, 0, shadowMap->getWidth(), shadowMap->getHeight());
    checkGLErrors("glViewport");
    glClearColor(1, 1, 1, 1);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glFrontFace(GL_CCW);
    renderShadowMeshes(m_pShadowVisitor->m_meshes,
                       Matrix4x4(light.getNode()->getWorldToLocalMatrix()), proj, 1.0f / range,
                       staticOnly, RenderableShaderGL::ShadowPass);
    glFrontFace(GL_CW);
    checkGLErrors("spotlight shadow");
}

// 0x004d75e0: the plane through three points, its normal on the side the points wind
// counter clockwise around.
static Plane planeThroughPoints(const Vec3& a, const Vec3& b, const Vec3& c)
{
    return Plane(normalize(cross(b - a, c - a)), a);
}

// 0x004d7e00: the volume that can cast a shadow into one cascade of the view frustum:
// the frustum planes of the camera that face away from the light, plus the silhouette
// edges of the slice extruded along the light direction. Returns the number of planes
// (at most MaxShadowCasterPlanes). The original shares the function between the GL and
// the Direct3D light pre-pass renderers.
static int getShadowCasterPlanes(const LightEntity& light, const Camera& camera, float cascadeStart,
                                 float cascadeEnd, Plane* planes)
{
    // the corners of the near plane, counter clockwise from the top left
    static constexpr float nearPlaneCorners[4][4] = {{-1.0f, 1.0f, 0.0f, 1.0f},
                                                     {1.0f, 1.0f, 0.0f, 1.0f},
                                                     {1.0f, -1.0f, 0.0f, 1.0f},
                                                     {-1.0f, -1.0f, 0.0f, 1.0f}};
    // the corners 0..3 are the near quad of the slice, 4..7 the far one; each face is
    // three of them in winding order and each edge carries the two faces it joins
    static constexpr int faces[6][3] = {{1, 2, 3}, {6, 5, 4}, {4, 5, 1},
                                        {7, 3, 2}, {4, 0, 3}, {5, 6, 2}};
    static constexpr int edges[12][4] = {{0, 1, 0, 2}, {1, 2, 0, 5}, {2, 3, 0, 3}, {3, 0, 0, 4},
                                         {4, 5, 1, 2}, {5, 6, 1, 5}, {6, 7, 1, 3}, {7, 4, 1, 4},
                                         {0, 4, 2, 4}, {1, 5, 2, 5}, {2, 6, 3, 5}, {3, 7, 3, 4}};

    Vec3 lightDirection = light.getNode()->getLocalToWorldMatrix().z;
    int numPlanes = 0;
    for (int i = 0; i < 6; ++i)
    {
        const Plane& plane = camera.getPlane(i);
        if (dot(plane.normal, lightDirection) < 0.0f)
            planes[numPlanes++] = plane;
    }

    Vec3 corners[8];
    const Matrix4x4& invProj = camera.getInverseProjectionMatrix();
    float nearScale = cascadeStart / camera.getNear();
    float farScale = cascadeEnd / camera.getNear();
    for (int i = 0; i < 4; ++i)
    {
        Vec4 corner = invProj.transform(Vec4(nearPlaneCorners[i][0], nearPlaneCorners[i][1],
                                             nearPlaneCorners[i][2], nearPlaneCorners[i][3]));
        Vec3 direction(corner.x / corner.w, corner.y / corner.w, corner.z / corner.w);
        corners[i] = direction * nearScale;
        corners[i + 4] = direction * farScale;
    }
    const Matrix4x3& localToWorld = camera.getLocalToWorldMatrix();
    for (int i = 0; i < 8; ++i)
        corners[i] = localToWorld.transformPoint(corners[i]);

    Vec3 faceNormals[6];
    for (int i = 0; i < 6; ++i)
    {
        const Vec3& a = corners[faces[i][0]];
        const Vec3& b = corners[faces[i][1]];
        const Vec3& c = corners[faces[i][2]];
        faceNormals[i] = normalize(cross(c - b, a - b));
    }

    Vec3 center = (corners[0] + corners[6]) * 0.5f;
    for (int i = 0; i < 12; ++i)
    {
        float front = dot(faceNormals[edges[i][2]], lightDirection);
        float back = dot(faceNormals[edges[i][3]], lightDirection);
        if ((front > 0.0f && back < 0.0f) || (front < 0.0f && back > 0.0f))
        {
            const Vec3& a = corners[edges[i][0]];
            const Vec3& b = corners[edges[i][1]];
            Vec3 extruded = a + lightDirection;
            Vec3 normal = cross(b - a, extruded - a);
            if (dot(normal, normal) <= 0.0f)
                continue; // a degenerate edge, the light runs along it
            Plane plane = planeThroughPoints(a, b, extruded);
            if (plane.distance(center) < 0.0f)
                plane = Plane(plane.normal * -1.0f, -plane.d);
            planes[numPlanes++] = plane;
        }
    }
    return numPlanes;
}

// 0x004ecad0: orthographic map of the sphere around one cascade of the view frustum,
// with the translation snapped to shadow map texels.
void LightPrePassRendererGL::renderDirectionalLightShadowMap(
    const LightEntity& light, const Camera& camera, Texture2DGL* shadowMap, GLuint depthBuffer,
    float cascadeStart, float cascadeEnd, Matrix4x4& shadowViewProj)
{
    ProfileScope profile("_RenderDirectionalLightShadowMap");
    // radius of the sphere around the camera that encloses the slice: the corner of the
    // near plane (z = 0 in the projection of the original) scaled out to the far end of
    // the slice
    const Matrix4x4& invProj = camera.getInverseProjectionMatrix();
    Vec4 corner = invProj.transform(Vec4(1.0f, 1.0f, 0.0f, 1.0f));
    float scale = cascadeEnd / (camera.getNear() * corner.w);
    Vec3 farCorner(corner.x * scale, corner.y * scale, corner.z * scale);
    float radius = farCorner.length();
    Vec3 center = camera.getLocalToWorldMatrix().pos;
    // light space bounds of the sphere, extended towards the light
    const Matrix4x3& worldToLight = light.getNode()->getWorldToLocalMatrix();
    Vec3 lightCenter = worldToLight.transformPoint(center);
    Vec3 mn = lightCenter - Vec3(radius, radius, radius + DirectionalShadowNearExtension);
    Vec3 mx = lightCenter + Vec3(radius, radius, radius);
    Matrix4x4 ortho;
    makeOrthoProjectionMatrix(&ortho, mn, mx);
    shadowViewProj = ortho * Matrix4x4(worldToLight);
    float halfSize = (float)(shadowMap->getWidth() / 2);
    shadowViewProj.m[12] = floorf(shadowViewProj.m[12] * halfSize) / halfSize;
    shadowViewProj.m[13] = floorf(shadowViewProj.m[13] * halfSize) / halfSize;
    Matrix4x4 flipY;
    flipY.m[5] = -1.0f;
    Plane planes[MaxShadowCasterPlanes];
    int numPlanes = getShadowCasterPlanes(light, camera, cascadeStart, cascadeEnd, planes);
    m_pShadowVisitor->gatherShadowCasters(*light.getNode()->getScene(), camera, planes, numPlanes);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    m_pContext->setBlendMode(RenderContextGL::Blend_Opaque);
    m_pContext->setRenderTarget(shadowMap, 0, depthBuffer, 0);
    glViewport(0, 0, shadowMap->getWidth(), shadowMap->getHeight());
    checkGLErrors("glViewport");
    glClearColor(1, 1, 1, 1);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glFrontFace(GL_CCW);
    Matrix4x4 proj = RenderContextGL::sm_d3dToGLProj * flipY;
    renderShadowMeshes(m_pShadowVisitor->m_meshes, shadowViewProj, proj, 0.0f, false,
                       RenderableShaderGL::ShadowDirLight);
    glFrontFace(GL_CW);
}

// 0x004ed600
TextureGL* LightPrePassRendererGL::renderStaticShadowMap(LightEntity& light, int size,
                                                         float* extents)
{
    float range = light.getLightRange();
    Camera lightCamera(HALF_PI, 1.0f, StaticShadowNearZ, range);
    lightCamera.setPosition(light.getNode()->getLocalToWorldMatrix().pos);
    Camera* previous = m_pContext->getCamera();
    m_pContext->setScene(m_pContext->getScene(), &lightCamera);
    GLuint depthBuffer = getShadowDepthBuffer(size);
    TextureGL* result;
    if (light.getLightType() == LightEntity::Point)
    {
        unsigned int faceMask = 0;
        for (int face = 0; face < LightEntity::NumClipDistances; ++face)
            if (light.getClipDistance(face) > 0.0f)
                faceMask |= 1u << face;
        TextureCubeGL* cube = new TextureCubeGL(size, 1, GL_RG16);
        TextureCubeGL *pooled, *temp;
        getShadowCubeMap(size, pooled, temp);
        renderPointLightShadowMap(light, lightCamera, temp, depthBuffer, true, faceMask);
        blurCubeMapGPU(temp, cube, StaticShadowBlurAngle, faceMask);
        result = cube;
    }
    else
    {
        Texture2DGL* map = new Texture2DGL(size, size, 1, GL_RG16, GL_LINEAR, GL_LINEAR,
                                           GL_CLAMP_TO_EDGE, GL_RGBA);
        renderSpotLightShadowMap(light, lightCamera, map, depthBuffer, true);
        result = map;
    }
    m_pContext->setScene(m_pContext->getScene(), previous);
    if (extents)
    {
        // how far the rendered geometry reaches per face: the largest depth along the
        // face's rays, converted to the distance along the face axis
        unsigned short* pixels = new unsigned short[size * size * 2];
        int faces = light.getLightType() == LightEntity::Point ? 6 : 1;
        for (int face = 0; face < faces; ++face)
        {
            GLenum target;
            if (light.getLightType() == LightEntity::Point)
            {
                glBindTexture(GL_TEXTURE_CUBE_MAP, result->getHandle());
                target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, result->getHandle());
                target = GL_TEXTURE_2D;
            }
            glGetTexImage(target, 0, GL_RG, GL_UNSIGNED_SHORT, pixels);
            float maxDepth = 0.0f;
            for (int y = 0; y < size; ++y)
            {
                float fy = (float)y / (float)(size - 1) * 2.0f - 1.0f;
                for (int x = 0; x < size; ++x)
                {
                    float fx = (float)x / (float)(size - 1) * 2.0f - 1.0f;
                    float d =
                        pixels[(y * size + x) * 2] / 65535.0f / sqrtf(fx * fx + fy * fy + 1.0f);
                    if (d > maxDepth)
                        maxDepth = d;
                }
            }
            float extent = range * maxDepth;
            if (extent < 0.0f)
                extent = 0.0f;
            if (extent > range)
                extent = range;
            extents[face] = extent;
        }
        delete[] pixels;
    }
    checkGLErrors("renderStaticShadowMap");
    return result;
}

// 0x004e5e00: 16 jittered samples in a cone of blurAngle around each texel direction.
void LightPrePassRendererGL::blurCubeMapGPU(TextureCubeGL* source, TextureCubeGL* target,
                                            float blurAngle, unsigned int faceMask)
{
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    m_pContext->setBlendMode(RenderContextGL::Blend_Opaque);
    glDisable(GL_CULL_FACE);
    m_pContext->useProgram(m_pBlurCubeMapProgram);
    GLuint program = m_pBlurCubeMapProgram->getProgram();
    MersenneTwister rng;
    Vec3 samples[16];
    for (int i = 0; i < 16; ++i)
    {
        float r1 = (float)rng.genrand_int31() / 2147483647.0f;
        float r2 = (float)rng.genrand_int31() / 2147483647.0f;
        Matrix3x3 rot;
        rot.makeRotation(r2 * blurAngle - blurAngle * 0.5f, r1 * TWO_PI - PI, 0.0f, Matrix3x3::XYZ);
        samples[i] = rot.y;
        if (blurAngle == 0.0f)
            samples[i].set(0.0f, 1.0f, 0.0f);
    }
    m_pContext->setUniformTexture("g_tex", source, -1, -1, 0);
    glUniform3fv(glGetUniformLocation(program, "g_samples"), 16, &samples[0].x);
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
        m_pContext->setRenderTarget(target, face, 0, 0);
        glViewport(0, 0, target->getWidth(), target->getHeight());
        checkGLErrors("glViewport");
        if (!(faceMask & (1u << face)))
        {
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            continue;
        }
        Matrix3x3 rot = faceRotations[face];
        rot.transpose();
        glUniformMatrix3fv(glGetUniformLocation(program, "g_faceRot"), 1, GL_FALSE, &rot.x.x);
        m_pContext->drawRect();
    }
}

} // namespace engine
