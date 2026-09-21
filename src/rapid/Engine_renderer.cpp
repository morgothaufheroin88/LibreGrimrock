// Renderer, Camera, CameraControls and the renderable resource bindings of Engine.cpp.
#include "EngineBindings.h"
#include "Frame.h"
#include "core/Exception.h"
#include "sys.h" // core LUAX_CLASS declarations (Image, Texture ...)

using namespace core;
using namespace engine;

// ---- Renderer --------------------------------------------------------------------

// 0x08145a70
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
// 0x08145a20
static int Renderer_active(lua_State* L)
{
    if (!g_pEngineSystems)
        luaL_error(L, "engine not initialized");
    pushSharedObject<Renderer>(L, Renderer::sm_pActiveRenderer);
    return 1;
}
// 0x0813ce70
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
// 0x08146430: array of vec(width, height, 0, 0)
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
// 0x0813b550
static int Renderer_setClearColor(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    renderer->m_clearColor = luax::checkColor(L, 2);
    return 0;
}
// 0x0814e310
static int Renderer_setWireframe(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    renderer->m_wireframe = luax::checkBool(L, 2);
    return 0;
}
// 0x0814e2b0
static int Renderer_setAmbientOcclusion(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    renderer->m_ambientOcclusion = luax::checkBool(L, 2);
    return 0;
}
// 0x0813b220
static int Renderer_setSSAOQuality(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    renderer->m_ssaoQuality = luaL_checkinteger(L, 2);
    return 0;
}
// 0x0814e250
static int Renderer_setFog(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    renderer->m_fog = luax::checkBool(L, 2);
    return 0;
}
// 0x0813b5a0
static int Renderer_setFogColor(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    renderer->m_fogColor = luax::checkVector3_alt(L, 2);
    return 0;
}
// 0x0813b470: {start, end}
static int Renderer_setFogRange(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    if (lua_type(L, 2) != LUA_TTABLE)
        luaL_typerror(L, 2, "table");
    lua_rawgeti(L, 2, 1);
    float start = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);
    lua_rawgeti(L, 2, 2);
    float end = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);
    renderer->m_fogStart = start;
    renderer->m_fogEnd = end;
    return 0;
}
// 0x0814e1f0
static int Renderer_setDiffuseMapping(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    renderer->m_diffuseMapping = luax::checkBool(L, 2);
    return 0;
}
// 0x0814e190
static int Renderer_setNormalMapping(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    renderer->m_normalMapping = luax::checkBool(L, 2);
    return 0;
}
// 0x0813b1e0
static int Renderer_setTextureFilter(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    renderer->m_textureFilter = luaL_checkinteger(L, 2);
    return 0;
}
// 0x0814e130
static int Renderer_setFXAA(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    renderer->m_fxaa = luax::checkBool(L, 2);
    return 0;
}
// 0x0814e0d0
static int Renderer_setRenderMeshes(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    renderer->m_renderMeshes = luax::checkBool(L, 2);
    return 0;
}
// 0x0814e3d0
static int Renderer_setRenderShadows(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    renderer->m_renderShadows = luax::checkBool(L, 2);
    return 0;
}
// 0x0813b1a0
static int Renderer_setShadowQuality(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    renderer->m_shadowQuality = luaL_checkinteger(L, 2);
    return 0;
}
// 0x0814e370
static int Renderer_setDrawNormalBuffer(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    renderer->m_drawNormalBuffer = luax::checkBool(L, 2);
    return 0;
}
// 0x0814e430
static int Renderer_setDrawGlossinessBuffer(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    renderer->m_drawGlossinessBuffer = luax::checkBool(L, 2);
    return 0;
}
// 0x0814d040
static int Renderer_setDrawLightBuffer(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    renderer->m_drawLightBuffer = luax::checkBool(L, 2);
    return 0;
}
// 0x0814cf10
static int Renderer_setDrawAmbientOcclusionBuffer(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    renderer->m_drawAmbientOcclusionBuffer = luax::checkBool(L, 2);
    return 0;
}
// 0x0814ceb0
static int Renderer_setDrawStats(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    renderer->m_drawStats = luax::checkBool(L, 2);
    return 0;
}
// 0x0813b110
static int Renderer_setViewport(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    int x = luaL_checkinteger(L, 2);
    int y = luaL_checkinteger(L, 3);
    int width = luaL_checkinteger(L, 4);
    int height = luaL_checkinteger(L, 5);
    renderer->m_viewportX = x;
    renderer->m_viewportY = y;
    renderer->m_viewportWidth = width;
    renderer->m_viewportHeight = height;
    return 0;
}
// 0x0813b3b0: vec(r, g, b, a) with the raw byte values
static int Renderer_getClearColor(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    const Color& c = renderer->m_clearColor;
    luax::pushVector(L, Vec4((float)c.r, (float)c.g, (float)c.b, (float)c.a));
    return 1;
}
// 0x0813aef0
static int Renderer_getWireframe(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<Renderer>(L, 1)->m_wireframe);
    return 1;
}
// 0x0813aeb0
static int Renderer_getAmbientOcclusion(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<Renderer>(L, 1)->m_ambientOcclusion);
    return 1;
}
// 0x0813a560
static int Renderer_getSSAOQuality(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<Renderer>(L, 1)->m_ssaoQuality);
    return 1;
}
// 0x0813ae70
static int Renderer_getFog(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<Renderer>(L, 1)->m_fog);
    return 1;
}
// 0x0813d610
static int Renderer_getFogColor(lua_State* L)
{
    luax::pushVector(L, luax::checkObject<Renderer>(L, 1)->m_fogColor);
    return 1;
}
// 0x08142fd0
static int Renderer_getFogRange(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    lua_createtable(L, 0, 0);
    lua_pushnumber(L, renderer->m_fogStart);
    lua_rawseti(L, -2, 1);
    lua_pushnumber(L, renderer->m_fogEnd);
    lua_rawseti(L, -2, 2);
    return 1;
}
// 0x0813ae30
static int Renderer_getDiffuseMapping(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<Renderer>(L, 1)->m_diffuseMapping);
    return 1;
}
// 0x0813adf0
static int Renderer_getNormalMapping(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<Renderer>(L, 1)->m_normalMapping);
    return 1;
}
// 0x0813a520
static int Renderer_getTextureFilter(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<Renderer>(L, 1)->m_textureFilter);
    return 1;
}
// 0x0813adb0
static int Renderer_getFXAA(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<Renderer>(L, 1)->m_fxaa);
    return 1;
}
// 0x0813ad70
static int Renderer_getRenderMeshes(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<Renderer>(L, 1)->m_renderMeshes);
    return 1;
}
// 0x0813ad30
static int Renderer_getRenderShadows(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<Renderer>(L, 1)->m_renderShadows);
    return 1;
}
// 0x0813a4e0
static int Renderer_getShadowQuality(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<Renderer>(L, 1)->m_shadowQuality);
    return 1;
}
// 0x0813acf0
static int Renderer_getDrawNormalBuffer(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<Renderer>(L, 1)->m_drawNormalBuffer);
    return 1;
}
// 0x0813acb0
static int Renderer_getDrawGlossinessBuffer(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<Renderer>(L, 1)->m_drawGlossinessBuffer);
    return 1;
}
// 0x0813ac70
static int Renderer_getDrawLightBuffer(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<Renderer>(L, 1)->m_drawLightBuffer);
    return 1;
}
// 0x0813ac30
static int Renderer_getDrawAmbientOcclusionBuffer(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<Renderer>(L, 1)->m_drawAmbientOcclusionBuffer);
    return 1;
}
// 0x0813abf0
static int Renderer_getDrawStats(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<Renderer>(L, 1)->m_drawStats);
    return 1;
}
// 0x0813b0a0
static int Renderer_resizeRenderBuffers(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    int width = luaL_checkinteger(L, 2);
    int height = luaL_checkinteger(L, 3);
    renderer->resizeRenderBuffers(width, height);
    return 0;
}
// 0x0813ab70
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
// 0x0813a5d0
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
// 0x0813a4b0
static int Renderer_beginRender(lua_State* L)
{
    luax::checkObject<Renderer>(L, 1)->beginRender();
    return 0;
}
// 0x0813ab00
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
// 0x0813fe60
static int Renderer_renderScene(lua_State* L)
{
    try
    {
        Renderer* renderer = luax::checkObject<Renderer>(L, 1);
        Scene* scene = luax::checkObject<Scene>(L, 2);
        Camera* camera = luax::checkObject<Camera>(L, 3);
        RenderableTexture* target = luax::checkObjectOpt<RenderableTexture>(L, 4);
        renderer->renderScene(*scene, *camera, target);
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x0813fcc0
static int Renderer_renderSceneAndSimulatePhysics(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    Scene* scene = luax::checkObject<Scene>(L, 2);
    Camera* camera = luax::checkObject<Camera>(L, 3);
    DynamicsWorld* world = luax::checkObject<DynamicsWorld>(L, 4);
    world->beginFrame();
    renderer->renderScene(*scene, *camera, 0);
    world->endFrame();
    return 0;
}
// 0x0813aa60
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
// 0x0813a460
static int Renderer_getAvailableTextureMemory(lua_State* L)
{
    Renderer* renderer = luax::checkObject<Renderer>(L, 1);
    lua_pushnumber(L, (unsigned)renderer->getAvailableTextureMemory());
    return 1;
}

const luaL_Reg Renderer_methods[] = {
    {"create", Renderer_create},
    {"active", Renderer_active},
    {"getRendererInfo", Renderer_getRendererInfo},
    {"enumerateResolutions", Renderer_enumerateResolutions},
    {"setClearColor", Renderer_setClearColor},
    {"setWireframe", Renderer_setWireframe},
    {"setAmbientOcclusion", Renderer_setAmbientOcclusion},
    {"setSSAOQuality", Renderer_setSSAOQuality},
    {"setFog", Renderer_setFog},
    {"setFogColor", Renderer_setFogColor},
    {"setFogRange", Renderer_setFogRange},
    {"setDiffuseMapping", Renderer_setDiffuseMapping},
    {"setNormalMapping", Renderer_setNormalMapping},
    {"setTextureFilter", Renderer_setTextureFilter},
    {"setFXAA", Renderer_setFXAA},
    {"setRenderMeshes", Renderer_setRenderMeshes},
    {"setRenderShadows", Renderer_setRenderShadows},
    {"setShadowQuality", Renderer_setShadowQuality},
    {"setDrawNormalBuffer", Renderer_setDrawNormalBuffer},
    {"setDrawGlossinessBuffer", Renderer_setDrawGlossinessBuffer},
    {"setDrawLightBuffer", Renderer_setDrawLightBuffer},
    {"setDrawAmbientOcclusionBuffer", Renderer_setDrawAmbientOcclusionBuffer},
    {"setDrawStats", Renderer_setDrawStats},
    {"setViewport", Renderer_setViewport},
    {"getClearColor", Renderer_getClearColor},
    {"getWireframe", Renderer_getWireframe},
    {"getAmbientOcclusion", Renderer_getAmbientOcclusion},
    {"getSSAOQuality", Renderer_getSSAOQuality},
    {"getFog", Renderer_getFog},
    {"getFogColor", Renderer_getFogColor},
    {"getFogRange", Renderer_getFogRange},
    {"getDiffuseMapping", Renderer_getDiffuseMapping},
    {"getNormalMapping", Renderer_getNormalMapping},
    {"getTextureFilter", Renderer_getTextureFilter},
    {"getFXAA", Renderer_getFXAA},
    {"getRenderMeshes", Renderer_getRenderMeshes},
    {"getRenderShadows", Renderer_getRenderShadows},
    {"getShadowQuality", Renderer_getShadowQuality},
    {"getDrawNormalBuffer", Renderer_getDrawNormalBuffer},
    {"getDrawGlossinessBuffer", Renderer_getDrawGlossinessBuffer},
    {"getDrawLightBuffer", Renderer_getDrawLightBuffer},
    {"getDrawAmbientOcclusionBuffer", Renderer_getDrawAmbientOcclusionBuffer},
    {"getDrawStats", Renderer_getDrawStats},
    {"resizeRenderBuffers", Renderer_resizeRenderBuffers},
    {"isReadyToRender", Renderer_isReadyToRender},
    {"setRenderWindow", Renderer_setRenderWindow},
    {"beginRender", Renderer_beginRender},
    {"endRender", Renderer_endRender},
    {"renderScene", Renderer_renderScene},
    {"renderSceneAndSimulatePhysics", Renderer_renderSceneAndSimulatePhysics},
    {"saveScreenShot", Renderer_saveScreenShot},
    {"getAvailableTextureMemory", Renderer_getAvailableTextureMemory},
    {0, 0}};
const char* Renderer_properties[] = {"ClearColor",
                                     "Wireframe",
                                     "AmbientOcclusion",
                                     "SSAOQuality",
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
                                     "DrawStats",
                                     0};

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

const luaL_Reg Camera_methods[] = {
    {"create", Camera_create},
    {"setProjectionMatrix", Camera_setProjectionMatrix},
    {"getProjectionMatrix", Camera_getProjectionMatrix},
    {"getInverseProjectionMatrix", Camera_getInverseProjectionMatrix},
    {"getViewProjectionMatrix", Camera_getViewProjectionMatrix},
    {"getInverseViewProjectionMatrix", Camera_getInverseViewProjectionMatrix},
    {"getPlane", Camera_getPlane},
    {"getNear", Camera_getNear},
    {"getFar", Camera_getFar},
    {"getViewRay", Camera_getViewRay},
    {"getWorldRay", Camera_getWorldRay},
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

const luaL_Reg CameraControls_methods[] = {{"create", CameraControls_create},
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

const luaL_Reg RenderableMesh_methods[] = {{"create", RenderableMesh_create},
                                           {"load", RenderableMesh_load},
                                           {"init", RenderableMesh_init},
                                           {"getSourceData", RenderableMesh_getSourceData},
                                           {"getBoundingBox", RenderableMesh_getBoundingBox},
                                           {"getMaterials", RenderableMesh_getMaterials},
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
// 0x08150510: RenderableTexture.load(filename [, flags [, mipmaps]])
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
// 0x0813fb60
static int RenderableTexture_reload(lua_State* L)
{
    try
    {
        luax::checkObject<RenderableTexture>(L, 1)->reload();
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
    {"reload", RenderableTexture_reload},
    {"getWidth", RenderableTexture_getWidth},
    {"getHeight", RenderableTexture_getHeight},
    {"getTextureByFilename", RenderableTexture_getTextureByFilename},
    {0, 0}};

// ---- RenderableShader ------------------------------------------------------------

// 0x081470c0: createSurfaceShader(vertex, geometry, material, unlit, shadow)
static int RenderableShader_createSurfaceShader(lua_State* L)
{
    try
    {
        const char* vertexShader = luaL_optstring(L, 1, 0);
        const char* geometryShader = luaL_optstring(L, 2, 0);
        const char* materialShader = luaL_optstring(L, 3, 0);
        const char* unlitShader = luaL_optstring(L, 4, 0);
        const char* shadowShader = luaL_optstring(L, 5, 0);
        RenderableShader* shader = Renderer::sm_pActiveRenderer->createRenderableShader();
        shader->initSurfaceShader(vertexShader, geometryShader, materialShader, unlitShader,
                                  shadowShader);
        luax::createSharedObject<RenderableShader>(L, shader);
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x08146f80
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
    {"createSurfaceShader", RenderableShader_createSurfaceShader},
    {"createPostProcessShader", RenderableShader_createPostProcessShader},
    {0, 0}};

// ---- RenderBuffer ----------------------------------------------------------------

// 0x08147640: RenderBuffer.create(width, height), a RenderableTexture
static int RenderBuffer_create(lua_State* L)
{
    try
    {
        int width = luaL_checkinteger(L, 1);
        int height = luaL_checkinteger(L, 2);
        RenderableTexture* buffer = Renderer::sm_pActiveRenderer->createRenderBuffer(width, height);
        luax::createSharedObject<RenderableTexture>(L, buffer);
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x0813ca90: temporaries stay owned by the pool
static int RenderBuffer_getTemporary(lua_State* L)
{
    int width = luaL_checkinteger(L, 1);
    int height = luaL_checkinteger(L, 2);
    RenderableTexture* buffer = RenderBuffer::getTemporary(width, height);
    if (buffer)
    {
        luax::pushObject(L, buffer);
        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            luax::createObject<RenderableTexture>(L, buffer, 0);
        }
        return 1;
    }
    lua_pushnil(L);
    return 1;
}
// 0x0814ae20
static int RenderBuffer_release(lua_State* L)
{
    RenderableTexture* buffer = luax::checkObject<RenderableTexture>(L, 1);
    RenderBuffer::release(buffer);
    luax::disposeObject(L, 1);
    return 0;
}

const luaL_Reg RenderBuffer_methods[] = {{"create", RenderBuffer_create},
                                         {"getTemporary", RenderBuffer_getTemporary},
                                         {"release", RenderBuffer_release},
                                         {0, 0}};

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
