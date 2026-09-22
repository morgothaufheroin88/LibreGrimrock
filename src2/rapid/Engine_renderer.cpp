// Renderer, Camera, CameraControls, the renderable resources and the post process
// filter bindings of Legend of Grimrock 2 (Engine.cpp, 0x004128f0-0x00418c90).
#include "EngineBindings.h"
#include "Frame.h"
#include "core/Exception.h"
#include "engine/ParticleSystem.h"
#include "sys.h" // core LUAX_CLASS declarations (Image, Texture ...)

using namespace core;
using namespace engine;

// 0x0061a1a0
luax::Enum g_rendererFlags[] = {
    {"draw_normal_buffer", Renderer::Flag_DrawNormalBuffer},
    {"draw_glossiness_buffer", Renderer::Flag_DrawGlossinessBuffer},
    {"draw_light_buffer", Renderer::Flag_DrawLightBuffer},
    {"draw_ambient_occlusion_buffer", Renderer::Flag_DrawAmbientOcclusionBuffer},
    {"draw_occlusion_culling_buffer", Renderer::Flag_DrawOcclusionCullingBuffer},
    {"force_notebook_mode", Renderer::Flag_ForceNotebookMode},
    {0, 0}};
// 0x0061a260
luax::Enum g_fogModes[] = {{"linear", FogFilter::Fog_Linear},
                           {"exp", FogFilter::Fog_Exp},
                           {"linear_lit", FogFilter::Fog_LinearLit},
                           {"dense", FogFilter::Fog_Dense},
                           {0, 0}};
// 0x0061a248
luax::Enum g_renderBufferFormats[] = {{"rgba_8", 0}, {"rgba_16f", 1}, {0, 0}};
// 0x0061a958: the names of the render statistics in RenderStats order
static const char* const g_renderStatNames[] = {
    "visible_meshes",    "visible_lights", "visible_particle_systems",
    "visible_occluders", "draw_segments",  "shadow_segments",
    "render_triangles",  "bind_shader",    "bind_material"};

// Objects owned by the engine (filters, buffers) get a proxy that never deletes them.
template <class T> static void pushEngineObject(lua_State* L, T* object)
{
    if (!object)
    {
        lua_pushnil(L);
        return;
    }
    luax::pushObject(L, object);
    if (!lua_isnil(L, -1))
        return;
    lua_pop(L, 1);
    luax::createObject<T>(L, object, 0);
}

// ---- Renderer --------------------------------------------------------------------

// 0x004128f0
static int Renderer_create(lua_State* L)
{
    try
    {
        if (g_pEngineSystems)
            luaL_error(L, "renderer already created");
        int renderEngine = luax::checkEnum(L, 1, g_renderEngines);
        Renderer* renderer = Renderer::create(renderEngine);
        luax::createObject<Renderer>(L, renderer, luax::ProxyDestructor<Renderer>::destroy);
        pushSharedObject<Renderer>(L, renderer);
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x00412a70
static int Renderer_active(lua_State* L)
{
    if (!g_pEngineSystems)
        luaL_error(L, "engine not initialized");
    pushSharedObject<Renderer>(L, Renderer::sm_pActiveRenderer);
    return 1;
}
// 0x00412b00
static int Renderer_getRendererInfo(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    RendererInfo info = renderer->getRendererInfo();
    lua_createtable(L, 0, 0);
    lua_pushstring(L, info.vendor.c_str());
    lua_setfield(L, -2, "vendor");
    lua_pushstring(L, info.renderer.c_str());
    lua_setfield(L, -2, "renderer");
    return 1;
}
// 0x00412bd0
static int Renderer_getRenderStats(lua_State* L)
{
    const int* stats = &g_renderStats.visibleMeshes;
    lua_createtable(L, 0, 0);
    for (size_t i = 0; i < sizeof(g_renderStatNames) / sizeof(g_renderStatNames[0]); ++i)
    {
        lua_pushnumber(L, stats[i]);
        lua_setfield(L, -2, g_renderStatNames[i]);
    }
    return 1;
}
// 0x00412c40: array of vec(width, height, 0, 0)
static int Renderer_enumerateResolutions(lua_State* L)
{
    try
    {
        Renderer* renderer = luax::checkObject<Renderer>(L, 1);
        Array<std::pair<int, int>> resolutions;
        renderer->enumerateResolutions(resolutions);
        lua_createtable(L, 0, 0);
        for (int i = 0; i < resolutions.size(); ++i)
        {
            luax::pushVector(
                L, Vec4((float)resolutions[i].first, (float)resolutions[i].second, 0.0f, 0.0f));
            lua_rawseti(L, -2, i + 1);
        }
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x00412ec0
static int Renderer_setClearColor(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    renderer->m_clearColor = luax::checkColor(L, 2);
    return 0;
}
#define RENDERER_BOOL(name, field)                                                                 \
    static int Renderer_set##name(lua_State* L)                                                    \
    {                                                                                              \
        luax::checkObject<Renderer>(L, 1)->field = luax::checkBool(L, 2);                          \
        return 0;                                                                                  \
    }                                                                                              \
    static int Renderer_get##name(lua_State* L)                                                    \
    {                                                                                              \
        lua_pushboolean(L, luax::checkObject<Renderer>(L, 1)->field);                              \
        return 1;                                                                                  \
    }
#define RENDERER_INT(name, field)                                                                  \
    static int Renderer_set##name(lua_State* L)                                                    \
    {                                                                                              \
        luax::checkObject<Renderer>(L, 1)->field = luaL_checkinteger(L, 2);                        \
        return 0;                                                                                  \
    }                                                                                              \
    static int Renderer_get##name(lua_State* L)                                                    \
    {                                                                                              \
        lua_pushnumber(L, luax::checkObject<Renderer>(L, 1)->field);                               \
        return 1;                                                                                  \
    }
// 0x00412230-0x004128a0
RENDERER_BOOL(Wireframe, m_wireframe)
RENDERER_BOOL(AmbientOcclusion, m_ambientOcclusion)
RENDERER_BOOL(Fog, m_fog)
RENDERER_BOOL(DiffuseMapping, m_diffuseMapping)
RENDERER_BOOL(NormalMapping, m_normalMapping)
RENDERER_INT(TextureFilter, m_textureFilter)
RENDERER_BOOL(FXAA, m_fxaa)
RENDERER_BOOL(OcclusionCulling, m_occlusionCulling)
RENDERER_INT(OcclusionCullingResolutionDivider, m_occlusionCullingResolutionDivider)
RENDERER_BOOL(RenderMeshes, m_renderMeshes)
RENDERER_BOOL(RenderShadows, m_renderShadows)
RENDERER_INT(ShadowQuality, m_shadowQuality)
#undef RENDERER_BOOL
#undef RENDERER_INT
// 0x00412fc0: setFlag(flag, bool)
static int Renderer_setFlag(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    unsigned int flag = (unsigned int)luax::checkEnum(L, 2, g_rendererFlags);
    luaL_checktype(L, 3, LUA_TBOOLEAN);
    if (lua_toboolean(L, 3))
        renderer->m_flags |= flag;
    else
        renderer->m_flags &= ~flag;
    return 0;
}
// 0x00413030
static int Renderer_getFlag(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    unsigned int flag = (unsigned int)luax::checkEnum(L, 2, g_rendererFlags);
    lua_pushboolean(L, (renderer->m_flags & flag) != 0);
    return 1;
}
// 0x00412f10
static int Renderer_setViewport(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    renderer->m_viewportX = luaL_checkinteger(L, 2);
    renderer->m_viewportY = luaL_checkinteger(L, 3);
    renderer->m_viewportWidth = luaL_checkinteger(L, 4);
    renderer->m_viewportHeight = luaL_checkinteger(L, 5);
    return 0;
}
// 0x00412f80: vec(r, g, b, a) with the raw byte values
static int Renderer_getClearColor(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    const Color& c = renderer->m_clearColor;
    luax::pushVector(L, Vec4((float)c.r, (float)c.g, (float)c.b, (float)c.a));
    return 1;
}
// 0x00413090
static int Renderer_resizeRenderBuffers(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    int width = luaL_checkinteger(L, 2);
    int height = luaL_checkinteger(L, 3);
    renderer->resizeRenderBuffers(width, height);
    return 0;
}
// 0x004130e0
static int Renderer_isReadyToRender(lua_State* L)
{
    try
    {
        Renderer* renderer = luax::checkObject<Renderer>(L, 1);
        lua_pushboolean(L, renderer->isReadyToRender());
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x00412d80
static int Renderer_setRenderWindow(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    if (lua_type(L, 2) != LUA_TNIL)
    {
        RenderWindow* window = luax::checkObject<RenderWindow>(L, 2);
        renderer->setRenderWindow(window);
        return 0;
    }
    renderer->setRenderWindow(0);
    return 0;
}
// 0x00412df0: setRefractionBuffer(texture | nil)
static int Renderer_setRefractionBuffer(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    if (lua_type(L, 2) == LUA_TNIL)
    {
        renderer->setRefractionBuffer(0);
        return 0;
    }
    renderer->setRefractionBuffer(luax::checkObject<RenderableTexture>(L, 2));
    return 0;
}
// 0x00412e70
static int Renderer_setRefractionClipPlane(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    renderer->setRefractionClipPlane(luax::checkVector4_alt(L, 2));
    return 0;
}
// 0x00413180
static int Renderer_beginRender(lua_State* L)
{
    luax::checkObject<Renderer>(L, 1)->beginRender();
    return 0;
}
// 0x004131c0
static int Renderer_endRender(lua_State* L)
{
    try
    {
        luax::checkObject<Renderer>(L, 1)->endRender();
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x00413250: renderScene(scene, camera [, target [, passMask]])
static int Renderer_renderScene(lua_State* L)
{
    try
    {
        Renderer* renderer = luax::checkObject<Renderer>(L, 1);
        Scene* scene = luax::checkObject<Scene>(L, 2);
        Camera* camera = luax::checkObject<Camera>(L, 3);
        RenderableTexture* target = luax::checkObjectOpt<RenderableTexture>(L, 4);
        int passMask = -1;
        if (lua_gettop(L) > 4)
            passMask = luaL_checkinteger(L, 5);
        renderer->renderScene(*scene, *camera, target, passMask);
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x00413350
static int Renderer_renderSceneAndSimulatePhysics(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    Scene* scene = luax::checkObject<Scene>(L, 2);
    Camera* camera = luax::checkObject<Camera>(L, 3);
    DynamicsWorld* world = luax::checkObject<DynamicsWorld>(L, 4);
    world->beginFrame();
    renderer->renderScene(*scene, *camera, 0, -1);
    world->endFrame();
    return 0;
}
// 0x00413410
static int Renderer_saveScreenShot(lua_State* L)
{
    try
    {
        Renderer* renderer = luax::checkObject<Renderer>(L, 1);
        const char* filename = luaL_checkstring(L, 2);
        renderer->saveScreenShot(filename);
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x004134b0
static int Renderer_getAvailableTextureMemory(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    lua_pushnumber(L, (unsigned)renderer->getAvailableTextureMemory());
    return 1;
}
// 0x00413590 / 0x00413510 / 0x00413610: the filters of the active renderer
static int Renderer_getFogFilter(lua_State* L)
{
    pushEngineObject<FogFilter>(L, Renderer::sm_pActiveRenderer->getFogFilter());
    return 1;
}
static int Renderer_getSSAOFilter(lua_State* L)
{
    pushEngineObject<SSAOFilter>(L, Renderer::sm_pActiveRenderer->getSSAOFilter());
    return 1;
}
static int Renderer_getTonemapper(lua_State* L)
{
    pushEngineObject<Tonemapper>(L, Renderer::sm_pActiveRenderer->getTonemapper());
    return 1;
}
// 0x00413690
static int Renderer_getGeometryBuffer(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    pushEngineObject<RenderableTexture>(L, renderer->getGeometryBuffer());
    return 1;
}

const luaL_Reg Renderer_methods[] = {
    {"create", Renderer_create},
    {"active", Renderer_active},
    {"getRendererInfo", Renderer_getRendererInfo},
    {"getRenderStats", Renderer_getRenderStats},
    {"enumerateResolutions", Renderer_enumerateResolutions},
    {"setClearColor", Renderer_setClearColor},
    {"setWireframe", Renderer_setWireframe},
    {"setAmbientOcclusion", Renderer_setAmbientOcclusion},
    {"setFog", Renderer_setFog},
    {"setDiffuseMapping", Renderer_setDiffuseMapping},
    {"setNormalMapping", Renderer_setNormalMapping},
    {"setTextureFilter", Renderer_setTextureFilter},
    {"setFXAA", Renderer_setFXAA},
    {"setOcclusionCulling", Renderer_setOcclusionCulling},
    {"setOcclusionCullingResolutionDivider", Renderer_setOcclusionCullingResolutionDivider},
    {"setRenderMeshes", Renderer_setRenderMeshes},
    {"setRenderShadows", Renderer_setRenderShadows},
    {"setShadowQuality", Renderer_setShadowQuality},
    {"setFlag", Renderer_setFlag},
    {"setViewport", Renderer_setViewport},
    {"getClearColor", Renderer_getClearColor},
    {"getWireframe", Renderer_getWireframe},
    {"getAmbientOcclusion", Renderer_getAmbientOcclusion},
    {"getFog", Renderer_getFog},
    {"getDiffuseMapping", Renderer_getDiffuseMapping},
    {"getNormalMapping", Renderer_getNormalMapping},
    {"getTextureFilter", Renderer_getTextureFilter},
    {"getFXAA", Renderer_getFXAA},
    {"getOcclusionCulling", Renderer_getOcclusionCulling},
    {"getOcclusionCullingResolutionDivider", Renderer_getOcclusionCullingResolutionDivider},
    {"getRenderMeshes", Renderer_getRenderMeshes},
    {"getRenderShadows", Renderer_getRenderShadows},
    {"getShadowQuality", Renderer_getShadowQuality},
    {"getFlag", Renderer_getFlag},
    {"resizeRenderBuffers", Renderer_resizeRenderBuffers},
    {"isReadyToRender", Renderer_isReadyToRender},
    {"setRenderWindow", Renderer_setRenderWindow},
    {"setRefractionBuffer", Renderer_setRefractionBuffer},
    {"setRefractionClipPlane", Renderer_setRefractionClipPlane},
    {"beginRender", Renderer_beginRender},
    {"endRender", Renderer_endRender},
    {"renderScene", Renderer_renderScene},
    {"renderSceneAndSimulatePhysics", Renderer_renderSceneAndSimulatePhysics},
    {"saveScreenShot", Renderer_saveScreenShot},
    {"getAvailableTextureMemory", Renderer_getAvailableTextureMemory},
    {"getFogFilter", Renderer_getFogFilter},
    {"getSSAOFilter", Renderer_getSSAOFilter},
    {"getTonemapper", Renderer_getTonemapper},
    {"getGeometryBuffer", Renderer_getGeometryBuffer},
    {0, 0}};
const char* Renderer_properties[] = {"ClearColor",
                                     "Wireframe",
                                     "AmbientOcclusion",
                                     "Fog",
                                     "DiffuseMapping",
                                     "NormalMapping",
                                     "TextureFilter",
                                     "FXAA",
                                     "RenderMeshes",
                                     "RenderShadows",
                                     "ShadowQuality",
                                     "DrawNormalBuffer",
                                     "DrawGlossinessBuffer",
                                     "DrawLightBuffer",
                                     "DrawAmbientOcclusionBuffer",
                                     0};

// ---- VPXPlayer -------------------------------------------------------------------

// 0x00417fd0
static int VPXPlayer_create(lua_State* L)
{
    try
    {
        luax::createSharedObject<VPXPlayer>(L, Renderer::sm_pActiveRenderer->createVPXPlayer());
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x00418060
static int VPXPlayer_open(lua_State* L)
{
    try
    {
        VPXPlayer* player = luax::checkObject<VPXPlayer>(L, 1);
        player->open(luaL_checkstring(L, 2));
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x00418100
static int VPXPlayer_close(lua_State* L)
{
    luax::checkObject<VPXPlayer>(L, 1)->close();
    return 0;
}
// 0x00418140
static int VPXPlayer_update(lua_State* L)
{
    try
    {
        luax::checkObject<VPXPlayer>(L, 1)->update();
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x00418180
static int VPXPlayer_isDone(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<VPXPlayer>(L, 1)->isDone());
    return 1;
}
// 0x004181d0
static int VPXPlayer_getWidth(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<VPXPlayer>(L, 1)->getWidth());
    return 1;
}
// 0x00418230
static int VPXPlayer_getHeight(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<VPXPlayer>(L, 1)->getHeight());
    return 1;
}

const luaL_Reg VPXPlayer_methods[] = {
    {"create", VPXPlayer_create},       {"open", VPXPlayer_open},
    {"close", VPXPlayer_close},         {"update", VPXPlayer_update},
    {"isDone", VPXPlayer_isDone},       {"getWidth", VPXPlayer_getWidth},
    {"getHeight", VPXPlayer_getHeight}, {0, 0}};

// ---- SSAOFilter ------------------------------------------------------------------

// 0x00418290
static int SSAOFilter_setQuality(lua_State* L)
{
    luax::checkObject<SSAOFilter>(L, 1)->quality = luaL_checkinteger(L, 2);
    return 0;
}
// 0x00418320
static int SSAOFilter_setIntensity(lua_State* L)
{
    luax::checkObject<SSAOFilter>(L, 1)->intensity = (float)luaL_checknumber(L, 2);
    return 0;
}
// 0x004182d0
static int SSAOFilter_getQuality(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<SSAOFilter>(L, 1)->quality);
    return 1;
}
// 0x00418360
static int SSAOFilter_getIntensity(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<SSAOFilter>(L, 1)->intensity);
    return 1;
}

const luaL_Reg SSAOFilter_methods[] = {{"setQuality", SSAOFilter_setQuality},
                                       {"setIntensity", SSAOFilter_setIntensity},
                                       {"getQuality", SSAOFilter_getQuality},
                                       {"getIntensity", SSAOFilter_getIntensity},
                                       {0, 0}};

// ---- FogFilter -------------------------------------------------------------------

// 0x004183b0
static int FogFilter_setFogMode(lua_State* L)
{
    luax::checkObject<FogFilter>(L, 1)->m_fogMode = luax::checkEnum(L, 2, g_fogModes);
    return 0;
}
// 0x00418470
static int FogFilter_setFogColor(lua_State* L)
{
    luax::checkObject<FogFilter>(L, 1)->m_fogColor = luax::checkVector3_alt(L, 2);
    return 0;
}
// 0x00418b10: setFogRange(vec) or setFogRange(start, end)
static int FogFilter_setFogRange(lua_State* L)
{
    FogFilter* filter = luax::checkObject<FogFilter>(L, 1);
    if (lua_gettop(L) == 2)
    {
        filter->m_fogRange = luax::checkVector2(L, 2);
        return 0;
    }
    filter->m_fogRange.x = (float)luaL_checknumber(L, 2);
    filter->m_fogRange.y = (float)luaL_checknumber(L, 3);
    return 0;
}
// 0x00418550
static int FogFilter_setFogDensity(lua_State* L)
{
    luax::checkObject<FogFilter>(L, 1)->m_fogDensity = (float)luaL_checknumber(L, 2);
    return 0;
}
// 0x004185e0
static int FogFilter_setFogLightDirection(lua_State* L)
{
    luax::checkObject<FogFilter>(L, 1)->m_fogLightDirection = luax::checkVector3_alt(L, 2);
    return 0;
}
// 0x004186c0: setParticleTexture(texture | nil)
static int FogFilter_setParticleTexture(lua_State* L)
{
    FogFilter* filter = luax::checkObject<FogFilter>(L, 1);
    if (lua_type(L, 2) == LUA_TNIL)
        filter->m_particleTexture.reset(0);
    else
        filter->m_particleTexture.reset(luax::checkObject<RenderableTexture>(L, 2));
    return 0;
}
// 0x00418960: setParticles(flat array of x, y, z, size, phase per particle | nil)
static int FogFilter_setParticles(lua_State* L)
{
    FogFilter* filter = luax::checkObject<FogFilter>(L, 1);
    if (lua_type(L, 2) == LUA_TNIL)
    {
        filter->m_particles.resize(0);
        return 0;
    }
    int count = (int)lua_objlen(L, 2) / 5;
    filter->m_particles.resize(count);
    for (int i = 0; i < count; ++i)
    {
        float* v = &filter->m_particles[i].x;
        for (int k = 0; k < 5; ++k)
        {
            lua_rawgeti(L, 2, i * 5 + k + 1);
            v[k] = (float)lua_tonumber(L, -1);
            lua_pop(L, 1);
        }
    }
    return 0;
}
// 0x004187f0
static int FogFilter_setParticleSize(lua_State* L)
{
    luax::checkObject<FogFilter>(L, 1)->m_particleSize = (float)luaL_checknumber(L, 2);
    return 0;
}
// 0x00418880
static int FogFilter_setParticleColor(lua_State* L)
{
    luax::checkObject<FogFilter>(L, 1)->m_particleColor = luax::checkVector3_alt(L, 2);
    return 0;
}
// 0x00418400
static int FogFilter_getFogMode(lua_State* L)
{
    luax::pushEnum(L, luax::checkObject<FogFilter>(L, 1)->m_fogMode, g_fogModes);
    return 1;
}
// 0x00418510
static int FogFilter_getFogColor(lua_State* L)
{
    luax::pushVector(L, luax::checkObject<FogFilter>(L, 1)->m_fogColor);
    return 1;
}
// 0x00418b90: {start, end}
static int FogFilter_getFogRange(lua_State* L)
{
    FogFilter* filter = luax::checkObject<FogFilter>(L, 1);
    lua_createtable(L, 0, 0);
    lua_pushnumber(L, filter->m_fogRange.x);
    lua_rawseti(L, -2, 1);
    lua_pushnumber(L, filter->m_fogRange.y);
    lua_rawseti(L, -2, 2);
    return 1;
}
// 0x00418590
static int FogFilter_getFogDensity(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<FogFilter>(L, 1)->m_fogDensity);
    return 1;
}
// 0x00418680
static int FogFilter_getFogLightDirection(lua_State* L)
{
    luax::pushVector(L, luax::checkObject<FogFilter>(L, 1)->m_fogLightDirection);
    return 1;
}
// 0x004187b0
static int FogFilter_getParticleTexture(lua_State* L)
{
    pushSharedObject<RenderableTexture>(
        L, luax::checkObject<FogFilter>(L, 1)->m_particleTexture.get());
    return 1;
}
// 0x00418830
static int FogFilter_getParticleSize(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<FogFilter>(L, 1)->m_particleSize);
    return 1;
}
// 0x00418920
static int FogFilter_getParticleColor(lua_State* L)
{
    luax::pushVector(L, luax::checkObject<FogFilter>(L, 1)->m_particleColor);
    return 1;
}

const luaL_Reg FogFilter_methods[] = {{"setFogMode", FogFilter_setFogMode},
                                      {"setFogColor", FogFilter_setFogColor},
                                      {"setFogRange", FogFilter_setFogRange},
                                      {"setFogDensity", FogFilter_setFogDensity},
                                      {"setFogLightDirection", FogFilter_setFogLightDirection},
                                      {"setParticleTexture", FogFilter_setParticleTexture},
                                      {"setParticles", FogFilter_setParticles},
                                      {"setParticleSize", FogFilter_setParticleSize},
                                      {"setParticleColor", FogFilter_setParticleColor},
                                      {"getFogMode", FogFilter_getFogMode},
                                      {"getFogColor", FogFilter_getFogColor},
                                      {"getFogRange", FogFilter_getFogRange},
                                      {"getFogDensity", FogFilter_getFogDensity},
                                      {"getFogLightDirection", FogFilter_getFogLightDirection},
                                      {"getParticleTexture", FogFilter_getParticleTexture},
                                      {"getParticleSize", FogFilter_getParticleSize},
                                      {"getParticleColor", FogFilter_getParticleColor},
                                      {0, 0}};

// ---- Tonemapper ------------------------------------------------------------------

// 0x00418c10
static int Tonemapper_setSaturation(lua_State* L)
{
    luax::checkObject<Tonemapper>(L, 1)->saturation = (float)luaL_checknumber(L, 2);
    return 0;
}
// 0x00418c50
static int Tonemapper_getSaturation(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<Tonemapper>(L, 1)->saturation);
    return 1;
}

const luaL_Reg Tonemapper_methods[] = {{"setSaturation", Tonemapper_setSaturation},
                                       {"getSaturation", Tonemapper_getSaturation},
                                       {0, 0}};

// ---- Camera ----------------------------------------------------------------------

// 0x08144810: Camera.create(fov, aspect, near, far)
static int Camera_create(lua_State* L)
{
    float fov = (float)luaL_checknumber(L, 1);
    float aspect = (float)luaL_checknumber(L, 2);
    float nearZ = (float)luaL_checknumber(L, 3);
    float farZ = (float)luaL_checknumber(L, 4);
    Camera* camera = new Camera(fov, aspect, nearZ, farZ);
    luax::createSharedObject<Camera>(L, camera);
    return 1;
}
// 0x08140710
static int Camera_setProjectionMatrix(lua_State* L)
{
    Camera* camera = luax::checkObject<Camera>(L, 1);
    Matrix4x4 m = luax::checkMatrix4x4(L, 2);
    camera->setProjectionMatrix(m);
    return 0;
}
// 0x0814ffe0
static int Camera_getProjectionMatrix(lua_State* L)
{
    Camera* camera = luax::checkObject<Camera>(L, 1);
    luax::pushMatrix(L, camera->getProjectionMatrix());
    return 1;
}
// 0x0814f7d0
static int Camera_getInverseProjectionMatrix(lua_State* L)
{
    Camera* camera = luax::checkObject<Camera>(L, 1);
    luax::pushMatrix(L, camera->getInverseProjectionMatrix());
    return 1;
}
// 0x0814f3f0
static int Camera_getViewProjectionMatrix(lua_State* L)
{
    Camera* camera = luax::checkObject<Camera>(L, 1);
    luax::pushMatrix(L, camera->getViewProjectionMatrix());
    return 1;
}
// 0x0814f180
static int Camera_getInverseViewProjectionMatrix(lua_State* L)
{
    Camera* camera = luax::checkObject<Camera>(L, 1);
    luax::pushMatrix(L, camera->getInverseViewProjectionMatrix());
    return 1;
}
// 0x08140130: {n=vec, d=number}, planes numbered 1..6
static int Camera_getPlane(lua_State* L)
{
    Camera* camera = luax::checkObject<Camera>(L, 1);
    int index = luaL_checkinteger(L, 2);
    if (index < 1 || index > 6)
        luaL_argerror(L, 2, "plane index out of range");
    const Plane& plane = camera->getPlane(index - 1);
    lua_createtable(L, 0, 0);
    luax::pushVector(L, plane.normal);
    lua_setfield(L, -2, "n");
    lua_pushnumber(L, plane.d);
    lua_setfield(L, -2, "d");
    return 1;
}
// 0x081400c0
static int Camera_getNear(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<Camera>(L, 1)->getNear());
    return 1;
}
// 0x08140050
static int Camera_getFar(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<Camera>(L, 1)->getFar());
    return 1;
}
// 0x08146140: screen position given as {x, y}
static Vec2 checkScreenPos(lua_State* L, int index)
{
    if (lua_type(L, index) != LUA_TTABLE)
        luaL_typerror(L, index, "table");
    Vec2 pos(0.0f, 0.0f);
    lua_rawgeti(L, index, 1);
    pos.x = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);
    lua_rawgeti(L, index, 2);
    pos.y = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);
    return pos;
}
static int Camera_getViewRay(lua_State* L)
{
    Camera* camera = luax::checkObject<Camera>(L, 1);
    Vec2 p = checkScreenPos(L, 2);
    luax::pushRay(L, camera->getViewRay(p));
    return 1;
}
// 0x08145f80
static int Camera_getWorldRay(lua_State* L)
{
    Camera* camera = luax::checkObject<Camera>(L, 1);
    Vec2 p = checkScreenPos(L, 2);
    luax::pushRay(L, camera->getWorldRay(p));
    return 1;
}
// 0x0813ff70: nil when the point is behind the camera
static int Camera_projectWorldPoint(lua_State* L)
{
    Camera* camera = luax::checkObject<Camera>(L, 1);
    Vec3 worldPoint = luax::checkVector3(L, 2);
    Vec3 screenPoint = camera->projectWorldPoint(worldPoint);
    if (screenPoint.z >= 0.0f)
        luax::pushVector(L, screenPoint);
    else
        lua_pushnil(L);
    return 1;
}

// 0x00413930: setUserClipPlane(index 1..6, vec(nx, ny, nz, d))
static int Camera_setUserClipPlane(lua_State* L)
{
    Camera* camera = luax::checkObject<Camera>(L, 1);
    int index = luaL_checkinteger(L, 2) - 1;
    if (index < 0 || index >= Camera::NumUserClipPlanes)
        luaL_argerror(L, 2, "invalid plane index");
    Vec4 v = luax::checkVector4_alt(L, 3);
    camera->setUserClipPlane(index, Plane(Vec3(v.x, v.y, v.z), v.w));
    return 0;
}
// 0x004139c0
static int Camera_setUserClipPlaneMask(lua_State* L)
{
    Camera* camera = luax::checkObject<Camera>(L, 1);
    camera->setUserClipPlaneMask((unsigned int)luaL_checkinteger(L, 2));
    return 0;
}
// 0x004136d0
static int Camera_setInverseCulling(lua_State* L)
{
    luax::checkObject<Camera>(L, 1)->setInverseCulling(luax::checkBool(L, 2));
    return 0;
}
// 0x00413770
static int Camera_setLodFactor(lua_State* L)
{
    luax::checkObject<Camera>(L, 1)->setLodFactor((float)luaL_checknumber(L, 2));
    return 0;
}
// 0x00413b00: {n=vec, d=number}
static int Camera_getUserClipPlane(lua_State* L)
{
    Camera* camera = luax::checkObject<Camera>(L, 1);
    int index = luaL_checkinteger(L, 2) - 1;
    if (index < 0 || index >= Camera::NumUserClipPlanes)
        luaL_argerror(L, 2, "invalid plane index");
    const Plane& plane = camera->getUserClipPlane(index);
    lua_createtable(L, 0, 0);
    luax::pushVector(L, plane.normal);
    lua_setfield(L, -2, "n");
    lua_pushnumber(L, plane.d);
    lua_setfield(L, -2, "d");
    return 1;
}
// 0x00413bb0
static int Camera_getUserClipPlaneMask(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<Camera>(L, 1)->getUserClipPlaneMask());
    return 1;
}
// 0x00413720
static int Camera_getInverseCulling(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<Camera>(L, 1)->getInverseCulling());
    return 1;
}
// 0x004137b0
static int Camera_getLodFactor(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<Camera>(L, 1)->getLodFactor());
    return 1;
}

const luaL_Reg Camera_methods[] = {
    {"create", Camera_create},
    {"setProjectionMatrix", Camera_setProjectionMatrix},
    {"setUserClipPlane", Camera_setUserClipPlane},
    {"setUserClipPlaneMask", Camera_setUserClipPlaneMask},
    {"setInverseCulling", Camera_setInverseCulling},
    {"setLodFactor", Camera_setLodFactor},
    {"getProjectionMatrix", Camera_getProjectionMatrix},
    {"getInverseProjectionMatrix", Camera_getInverseProjectionMatrix},
    {"getViewProjectionMatrix", Camera_getViewProjectionMatrix},
    {"getInverseViewProjectionMatrix", Camera_getInverseViewProjectionMatrix},
    {"getUserClipPlane", Camera_getUserClipPlane},
    {"getUserClipPlaneMask", Camera_getUserClipPlaneMask},
    {"getInverseCulling", Camera_getInverseCulling},
    {"getPlane", Camera_getPlane},
    {"getNear", Camera_getNear},
    {"getFar", Camera_getFar},
    {"getViewRay", Camera_getViewRay},
    {"getWorldRay", Camera_getWorldRay},
    {"getLodFactor", Camera_getLodFactor},
    {"projectWorldPoint", Camera_projectWorldPoint},
    {0, 0}};

// ---- CameraControls --------------------------------------------------------------

// The Lua side wraps the controls in a derived class (vtable 0x08241b88) that adds
// nothing but its own destructor.
class CameraControlsEx : public CameraControls
{
  public:
    CameraControlsEx(Window* window, Camera* camera, float scale)
        : CameraControls(window, camera, scale)
    {
    }
    ~CameraControlsEx() {}
};
LUAX_CLASS(CameraControlsEx, "CameraControls")

// 0x08144ce0: CameraControls.create(camera [, scale])
static int CameraControls_create(lua_State* L)
{
    Camera* camera = luax::checkObject<Camera>(L, 1);
    float scale = (float)luaL_optnumber(L, 2, 1.0);
    CameraControlsEx* controls = new CameraControlsEx(g_pMainFrame->getWindow(), camera, scale);
    luax::createSharedObject<CameraControlsEx>(L, controls);
    return 1;
}
// 0x0813dc60
static int CameraControls_setScale(lua_State* L)
{
    CameraControlsEx* controls = luax::checkObject<CameraControlsEx>(L, 1);
    controls->setScale((float)luaL_checknumber(L, 2));
    return 0;
}
// 0x0813fdd0 (the original checks the camera at index 1, so it always failed)
static int CameraControls_setCamera(lua_State* L)
{
    CameraControlsEx* controls = luax::checkObject<CameraControlsEx>(L, 1);
    Camera* camera = luax::checkObject<Camera>(L, 2);
    controls->setCamera(camera);
    return 0;
}
// 0x0813dbf0
static int CameraControls_getScale(lua_State* L)
{
    CameraControlsEx* controls = luax::checkObject<CameraControlsEx>(L, 1);
    lua_pushnumber(L, controls->getScale());
    return 1;
}
// 0x0813dcd0
static int CameraControls_getCamera(lua_State* L)
{
    CameraControlsEx* controls = luax::checkObject<CameraControlsEx>(L, 1);
    luax::pushObject(L, controls->getCamera());
    return 1;
}
// 0x0813cb40
static int CameraControls_update(lua_State* L)
{
    CameraControlsEx* controls = luax::checkObject<CameraControlsEx>(L, 1);
    controls->update((float)luaL_checknumber(L, 2));
    return 0;
}

// 0x004140c0
static int CameraControls_setEnableControls(lua_State* L)
{
    CameraControlsEx* controls = luax::checkObject<CameraControlsEx>(L, 1);
    controls->setEnableControls(luax::checkBool(L, 2));
    return 0;
}
// 0x004141e0
static int CameraControls_getEnableControls(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<CameraControlsEx>(L, 1)->getEnableControls());
    return 1;
}

const luaL_Reg CameraControls_methods[] = {{"create", CameraControls_create},
                                           {"setEnableControls", CameraControls_setEnableControls},
                                           {"getEnableControls", CameraControls_getEnableControls},
                                           {"setScale", CameraControls_setScale},
                                           {"setCamera", CameraControls_setCamera},
                                           {"getScale", CameraControls_getScale},
                                           {"getCamera", CameraControls_getCamera},
                                           {"update", CameraControls_update},
                                           {0, 0}};

// ---- RenderableMesh --------------------------------------------------------------

// 0x08151040: RenderableMesh.create(mesh [, keepSourceData])
static int RenderableMesh_create(lua_State* L)
{
    Mesh* mesh = luax::checkObject<Mesh>(L, 1);
    bool keepSourceData = false;
    if (lua_type(L, 2) != LUA_TNONE)
        keepSourceData = luax::checkBool(L, 2);
    RenderableMesh* rmesh = createRenderableMesh(*mesh, keepSourceData);
    luax::createSharedObject<RenderableMesh>(L, rmesh);
    return 1;
}
// 0x0814e490: RenderableMesh.load(filename [, keepSourceData])
static int RenderableMesh_load(lua_State* L)
{
    try
    {
        const char* filename = luaL_checkstring(L, 1);
        bool keepSourceData = false;
        if (lua_type(L, 2) != LUA_TNONE)
            keepSourceData = luax::checkBool(L, 2);
        RenderableMesh* rmesh = loadRenderableMesh(filename, keepSourceData);
        luax::createSharedObject<RenderableMesh>(L, rmesh);
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x081506a0
static int RenderableMesh_init(lua_State* L)
{
    RenderableMesh* rmesh = luax::checkObject<RenderableMesh>(L, 1);
    Mesh* mesh = luax::checkObject<Mesh>(L, 2);
    bool keepSourceData = false;
    if (lua_type(L, 3) != LUA_TNONE)
        keepSourceData = luax::checkBool(L, 3);
    rmesh->init(*mesh, keepSourceData);
    return 0;
}
// 0x0814f660
static int RenderableMesh_getSourceData(lua_State* L)
{
    RenderableMesh* rmesh = luax::checkObject<RenderableMesh>(L, 1);
    if (rmesh->getSourceData())
    {
        Mesh* mesh = rmesh->getSourceData();
        if (mesh)
        {
            pushSharedObject<Mesh>(L, mesh);
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}
// 0x08142e20
static int RenderableMesh_getBoundingBox(lua_State* L)
{
    RenderableMesh* rmesh = luax::checkObject<RenderableMesh>(L, 1);
    luax::pushBox(L, rmesh->getBoundingBox());
    return 1;
}
// 0x081433a0
static int RenderableMesh_getMaterials(lua_State* L)
{
    RenderableMesh* rmesh = luax::checkObject<RenderableMesh>(L, 1);
    Array<SharedPtr<Material>> materials = rmesh->getMaterials();
    lua_createtable(L, 0, 0);
    for (int i = 0; i < materials.size(); ++i)
    {
        pushSharedObject<Material>(L, materials[i].get());
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

// 0x00417810
static int RenderableMesh_getFilename(lua_State* L)
{
    lua_pushstring(L, luax::checkObject<RenderableMesh>(L, 1)->getFilename().c_str());
    return 1;
}

const luaL_Reg RenderableMesh_methods[] = {{"create", RenderableMesh_create},
                                           {"load", RenderableMesh_load},
                                           {"init", RenderableMesh_init},
                                           {"getSourceData", RenderableMesh_getSourceData},
                                           {"getBoundingBox", RenderableMesh_getBoundingBox},
                                           {"getMaterials", RenderableMesh_getMaterials},
                                           {"getFilename", RenderableMesh_getFilename},
                                           {0, 0}};

// ---- RenderableTexture -----------------------------------------------------------

// 0x08147780: RenderableTexture.create(image)
static int RenderableTexture_create(lua_State* L)
{
    try
    {
        Image* image = luax::checkObject<Image>(L, 1);
        RenderableTexture* texture = createRenderableTexture(*image);
        luax::createSharedObject<RenderableTexture>(L, texture);
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x00417900: RenderableTexture.load(filename [, skipMipLevels [, srgb]])
static int RenderableTexture_load(lua_State* L)
{
    try
    {
        const char* filename = luaL_checkstring(L, 1);
        int flags = 0;
        if (lua_gettop(L) > 1)
            flags = luaL_checkinteger(L, 2);
        bool mipmaps = false;
        if (lua_gettop(L) > 2)
            mipmaps = luax::checkBool(L, 3);
        RenderableTexture* texture = loadRenderableTexture(filename, flags, mipmaps);
        luax::createSharedObject<RenderableTexture>(L, texture);
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x0813fc00
static int RenderableTexture_save(lua_State* L)
{
    try
    {
        RenderableTexture* texture = luax::checkObject<RenderableTexture>(L, 1);
        const char* filename = luaL_checkstring(L, 2);
        texture->save(filename);
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x0813fa50
static int RenderableTexture_getWidth(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<RenderableTexture>(L, 1)->getWidth());
    return 1;
}
// 0x0813f9e0
static int RenderableTexture_getHeight(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<RenderableTexture>(L, 1)->getHeight());
    return 1;
}
// 0x08143810
static int RenderableTexture_getTextureByFilename(lua_State* L)
{
    const char* filename = luaL_checkstring(L, 1);
    pushSharedObject<RenderableTexture>(L, RenderableTexture::getTextureByFilename(filename));
    return 1;
}

const luaL_Reg RenderableTexture_methods[] = {
    {"create", RenderableTexture_create},
    {"load", RenderableTexture_load},
    {"save", RenderableTexture_save},
    {"getWidth", RenderableTexture_getWidth},
    {"getHeight", RenderableTexture_getHeight},
    {"getTextureByFilename", RenderableTexture_getTextureByFilename},
    {0, 0}};

// ---- RenderableShader ------------------------------------------------------------

// 0x00417b70
static int RenderableShader_create(lua_State* L)
{
    RenderableShader* shader = Renderer::sm_pActiveRenderer->createRenderableShader();
    luax::createSharedObject<RenderableShader>(L, shader);
    return 1;
}
// 0x00417c00: initLightPrePassRendererShader(vertex, geometry, material, unlit, shadow)
static int RenderableShader_initLightPrePassRendererShader(lua_State* L)
{
    try
    {
        RenderableShader* shader = luax::checkObject<RenderableShader>(L, 1);
        const char* vertexShader = luaL_optstring(L, 2, 0);
        const char* geometryShader = luaL_optstring(L, 3, 0);
        const char* materialShader = luaL_optstring(L, 4, 0);
        const char* unlitShader = luaL_optstring(L, 5, 0);
        const char* shadowShader = luaL_optstring(L, 6, 0);
        shader->initLightPrePassRendererShader(vertexShader, geometryShader, materialShader,
                                               unlitShader, shadowShader);
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x00417cf0: initNotebookRendererShader(vertex, fragment, unlit)
static int RenderableShader_initNotebookRendererShader(lua_State* L)
{
    try
    {
        RenderableShader* shader = luax::checkObject<RenderableShader>(L, 1);
        const char* vertexShader = luaL_optstring(L, 2, 0);
        const char* fragmentShader = luaL_optstring(L, 3, 0);
        const char* unlitShader = luaL_optstring(L, 4, 0);
        shader->initNotebookRendererShader(vertexShader, fragmentShader, unlitShader);
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x00417dc0
static int RenderableShader_createPostProcessShader(lua_State* L)
{
    try
    {
        const char* fragmentShader = luaL_checkstring(L, 1);
        RenderableShader* shader = Renderer::sm_pActiveRenderer->createRenderableShader();
        shader->initPostProcessShader(fragmentShader);
        luax::createSharedObject<RenderableShader>(L, shader);
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}

const luaL_Reg RenderableShader_methods[] = {
    {"create", RenderableShader_create},
    {"initLightPrePassRendererShader", RenderableShader_initLightPrePassRendererShader},
    {"initNotebookRendererShader", RenderableShader_initNotebookRendererShader},
    {"createPostProcessShader", RenderableShader_createPostProcessShader},
    {0, 0}};

// ---- RenderBuffer ----------------------------------------------------------------

// 0x00417e60: RenderBuffer.create(width, height, format), a RenderableTexture
static int RenderBuffer_create(lua_State* L)
{
    try
    {
        int width = luaL_checkinteger(L, 1);
        int height = luaL_checkinteger(L, 2);
        int format = luax::checkEnum(L, 3, g_renderBufferFormats);
        RenderableTexture* buffer =
            Renderer::sm_pActiveRenderer->createRenderBuffer(width, height, format);
        luax::createSharedObject<RenderableTexture>(L, buffer);
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}

const luaL_Reg RenderBuffer_methods[] = {{"create", RenderBuffer_create}, {0, 0}};

// ---- RenderWindow ----------------------------------------------------------------

// 0x08144bb0: RenderWindow.create(frame)
static int RenderWindow_create(lua_State* L)
{
    try
    {
        Frame* frame = checkFrame(L, 1);
        RenderWindow* window =
            Renderer::sm_pActiveRenderer->createRenderWindow(*frame->getWindow());
        luax::createSharedObject<RenderWindow>(L, window);
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}

const luaL_Reg RenderWindow_methods[] = {{"create", RenderWindow_create}, {0, 0}};
