// Model, Material, MaterialLibrary and Font bindings of Engine.cpp.
#include "EngineBindings.h"
#include "core/Exception.h"
#include "engine/Graphics.h"
#include <cstring>

using namespace core;
using namespace engine;

#if GRIMROCK_GAME >= 2
// 0x0061a2c8
luax::Enum g_blendModes[] = {{"Opaque", 0},
                             {"Additive", 1},
                             {"Modulative", 2},
                             {"Translucent", 3},
                             {"Screen", 4},
                             {"AdditiveSrcAlpha", 5},
                             {"PremultipliedAlpha", 6},
                             {0, 0}};
#else
// 0x082be7e0
luax::Enum g_blendModes[] = {
    {"Opaque", 0}, {"Additive", 1}, {"Modulative", 2}, {"Translucent", 3}, {0, 0}};
#endif
// 0x082be820
luax::Enum g_textureFilterModes[] = {{"Nearest", 0},
                                     {"Linear", 1},
                                     {"Nearest_MipNearest", 2},
                                     {"Linear_MipNearest", 3},
                                     {"Linear_MipLinear", 4},
                                     {"Anisotropic", 5},
                                     {0, 0}};
#if GRIMROCK_GAME >= 2
// 0x0061a300
luax::Enum g_textureAddressModes[] = {{"Wrap", 0},
                                      {"Clamp", 1},
                                      {"WrapU_ClampV_WrapW", 2},
                                      {"ClampU_WrapV_WrapW", 3},
                                      {"WrapU_ClampV_ClampW", 4},
                                      {"ClampU_WrapV_ClampW", 5},
                                      {0, 0}};
#else
// 0x082be7c0
luax::Enum g_textureAddressModes[] = {{"Wrap", 0}, {"Clamp", 1}, {0, 0}};
#endif

static void pushEnumName(lua_State* L, const luax::Enum* enums, int value)
{
    for (const luax::Enum* e = enums; e->name; ++e)
    {
        if (e->value == value)
        {
            lua_pushstring(L, e->name);
            return;
        }
    }
    lua_pushstring(L, "???");
}

// ---- Model -----------------------------------------------------------------------

// 0x08151a30: Model.load(filename [, keepSourceData])
static int Model_load(lua_State* L)
{
    try
    {
        const char* filename = luaL_checkstring(L, 1);
        bool keepSourceData = false;
        if (lua_type(L, 2) != LUA_TNONE)
            keepSourceData = luax::checkBool(L, 2);
        Model* model = loadModel(filename, keepSourceData);
        luax::createSharedObject<Model>(L, model);
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x08150270: disposes the proxies of the model's nodes with the model
static int Model_dispose(lua_State* L)
{
    luax::Proxy* proxy = luax::checkProxy(L, 1);
    Model* model = (Model*)proxy->object;
    if (model)
    {
        const Array<Node*>& nodes = model->getNodes();
        for (int i = 0; i < nodes.size(); ++i)
        {
            luax::pushObject(L, nodes[i]);
            if (!lua_isnil(L, -1))
                luax::disposeObject(L, -1);
            lua_pop(L, 1);
        }
        luax::disposeObject(L, 1);
    }
    return 0;
}
// 0x081525c0
static int Model_instantiate(lua_State* L)
{
    Model* model = luax::checkObject<Model>(L, 1);
    Scene* scene = luax::checkObject<Scene>(L, 2);
    pushNode(L, model->instantiate(*scene));
    return 1;
}
// 0x0813d170
static int Model_getNodeCount(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<Model>(L, 1)->getNodes().size());
    return 1;
}
// 0x08152060
static int Model_getNode(lua_State* L)
{
    Model* model = luax::checkObject<Model>(L, 1);
    int index = luaL_checkinteger(L, 2);
    if (index < 1 || index > model->getNodes().size())
        luaL_argerror(L, 2, "node index out of range");
    pushNode(L, model->getNodes()[index - 1]);
    return 1;
}
// 0x08152160
static int Model_findNode(lua_State* L)
{
    Model* model = luax::checkObject<Model>(L, 1);
    const char* name = luaL_checkstring(L, 2);
    pushNode(L, model->getRootNode()->findNode(name));
    return 1;
}

const luaL_Reg Model_methods[] = {{"load", Model_load},
                                  {"__gc", Model_dispose},
                                  {"dispose", Model_dispose},
                                  {"instantiate", Model_instantiate},
                                  {"getNodeCount", Model_getNodeCount},
                                  {"getNode", Model_getNode},
                                  {"findNode", Model_findNode},
                                  {0, 0}};

// ---- Material --------------------------------------------------------------------

// 0x081465b0
static int Material_create(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    luax::createSharedObject<Material>(L, new Material(name));
    return 1;
}
// 0x08149cd0
static int Material_setName(lua_State* L)
{
    Material* material = luax::checkObject<Material>(L, 1);
    material->setName(luaL_checkstring(L, 2));
    return 0;
}
// 0x0814a170
static int Material_setDiffuseMap(lua_State* L)
{
    Material* material = luax::checkObject<Material>(L, 1);
    if (lua_type(L, 2) == LUA_TNIL)
    {
        material->setDiffuseMap(0);
        return 0;
    }
    material->setDiffuseMap(luax::checkObject<RenderableTexture>(L, 2));
    return 0;
}
// 0x0814a090
static int Material_setSpecularMap(lua_State* L)
{
    Material* material = luax::checkObject<Material>(L, 1);
    if (lua_type(L, 2) == LUA_TNIL)
    {
        material->setSpecularMap(0);
        return 0;
    }
    material->setSpecularMap(luax::checkObject<RenderableTexture>(L, 2));
    return 0;
}
// 0x08149fb0
static int Material_setNormalMap(lua_State* L)
{
    Material* material = luax::checkObject<Material>(L, 1);
    if (lua_type(L, 2) == LUA_TNIL)
    {
        material->setNormalMap(0);
        return 0;
    }
    material->setNormalMap(luax::checkObject<RenderableTexture>(L, 2));
    return 0;
}
#if GRIMROCK_GAME >= 2
// 0x0041dbd0: Material.clone(name) copies an existing material
static int Material_clone(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    Material* source = Material::findMaterialByName(name);
    if (!source)
        return luaL_error(L, "material not found '%s'", name);
    luax::createSharedObject<Material>(L, new Material(*source));
    return 1;
}
// 0x0041d2d0
static int Material_setEmissiveMap(lua_State* L)
{
    Material* material = luax::checkObject<Material>(L, 1);
    if (lua_type(L, 2) == LUA_TNIL)
    {
        material->setEmissiveMap(0);
        return 0;
    }
    material->setEmissiveMap(luax::checkObject<RenderableTexture>(L, 2));
    return 0;
}
// 0x0041d560
static int Material_setAmbientOcclusion(lua_State* L)
{
    Material* material = luax::checkObject<Material>(L, 1);
    material->setAmbientOcclusion(luax::checkBool(L, 2));
    return 0;
}
// 0x0041d610
static int Material_setCastShadow(lua_State* L)
{
    Material* material = luax::checkObject<Material>(L, 1);
    material->setCastShadow(luax::checkBool(L, 2));
    return 0;
}
// 0x0041d920
static int Material_setTexcoordScaleOffset(lua_State* L)
{
    Material* material = luax::checkObject<Material>(L, 1);
    material->setTexcoordScaleOffset(luax::checkVector4_alt(L, 2));
    return 0;
}
// 0x0041d3c0
static int Material_getEmissiveMap(lua_State* L)
{
    pushSharedObject<RenderableTexture>(L, luax::checkObject<Material>(L, 1)->getEmissiveMap());
    return 1;
}
// 0x0041d5c0
static int Material_getAmbientOcclusion(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<Material>(L, 1)->getAmbientOcclusion());
    return 1;
}
// 0x0041d670
static int Material_getCastShadow(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<Material>(L, 1)->getCastShadow());
    return 1;
}
// 0x0041d980
static int Material_getTexcoordScaleOffset(lua_State* L)
{
    luax::pushVector(L, luax::checkObject<Material>(L, 1)->getTexcoordScaleOffset());
    return 1;
}
#endif
// 0x0814a250
static int Material_setShader(lua_State* L)
{
    Material* material = luax::checkObject<Material>(L, 1);
    if (lua_type(L, 2) != LUA_TNIL)
    {
        material->setShader(luax::checkObject<RenderableShader>(L, 2));
        return 0;
    }
    material->setShader(0);
    return 0;
}
// 0x0814dfd0
static int Material_setDoubleSided(lua_State* L)
{
    Material* material = luax::checkObject<Material>(L, 1);
    material->setDoubleSided(luax::checkBool(L, 2));
    return 0;
}
// 0x0814df50
static int Material_setLighting(lua_State* L)
{
    Material* material = luax::checkObject<Material>(L, 1);
    material->setLighting(luax::checkBool(L, 2));
    return 0;
}
// 0x0814ded0
static int Material_setAlphaTest(lua_State* L)
{
    Material* material = luax::checkObject<Material>(L, 1);
    material->setAlphaTest(luax::checkBool(L, 2));
    return 0;
}
// 0x08149880
static int Material_setBlendMode(lua_State* L)
{
    Material* material = luax::checkObject<Material>(L, 1);
    material->setBlendMode((Material::BlendMode)luax::checkEnum(L, 2, g_blendModes));
    return 0;
}
// 0x08149810
static int Material_setTextureAddressMode(lua_State* L)
{
    Material* material = luax::checkObject<Material>(L, 1);
    material->setTextureAddressMode(
        (Material::AddressMode)luax::checkEnum(L, 2, g_textureAddressModes));
    return 0;
}
// 0x08149f40
static int Material_setGlossiness(lua_State* L)
{
    Material* material = luax::checkObject<Material>(L, 1);
    material->setGlossiness((float)luaL_checknumber(L, 2));
    return 0;
}
// 0x08149ed0
static int Material_setDepthBias(lua_State* L)
{
    Material* material = luax::checkObject<Material>(L, 1);
    material->setDepthBias((float)luaL_checknumber(L, 2));
    return 0;
}
// 0x08149a20: setTexture(name, texture | nil)
static int Material_setTexture(lua_State* L)
{
    Material* material = luax::checkObject<Material>(L, 1);
    const char* name = luaL_checkstring(L, 2);
    RenderableTexture* texture = luax::checkObjectOpt<RenderableTexture>(L, 3);
    if (!texture)
    {
        if (lua_type(L, 3) != LUA_TNIL)
        {
            luaL_typerror(L, 3, "RenderableTexture");
            return 0;
        }
        material->setTexture(name, 0);
    }
    else
    {
        material->setTexture(name, texture);
    }
    return 0;
}
// 0x08149bc0
static int Material_setTextureFilter(lua_State* L)
{
    Material* material = luax::checkObject<Material>(L, 1);
    const char* name = luaL_checkstring(L, 2);
    int filter = luax::checkEnum(L, 3, g_textureFilterModes);
    material->setTextureFilter(name, (Material::TextureFilter)filter);
    return 0;
}
// 0x08149b20
static int Material_setTextureAddress(lua_State* L)
{
    Material* material = luax::checkObject<Material>(L, 1);
    const char* name = luaL_checkstring(L, 2);
    int mode = luax::checkEnum(L, 3, g_textureAddressModes);
    material->setTextureAddress(name, (Material::AddressMode)mode);
    return 0;
}
// 0x081493d0: setParam(name, number) or setParam(name, table, components)
static int Material_setParam(lua_State* L)
{
    Material* material = luax::checkObject<Material>(L, 1);
    const char* name = luaL_checkstring(L, 2);
    if (!lua_isnumber(L, 3))
    {
        int components = luaL_checkinteger(L, 4);
        if (components < 1 || components > 4)
            luaL_argerror(L, 4, "invalid number of components");
        switch (components)
        {
        case 1:
        {
            if (lua_type(L, 3) != LUA_TTABLE)
                luaL_typerror(L, 3, "table");
            lua_rawgeti(L, 3, 1);
            float v = (float)lua_tonumber(L, -1);
            lua_pop(L, 1);
            material->setParam(name, v);
            break;
        }
        case 2:
            material->setParam(name, luax::checkVector2(L, 3));
            break;
        case 3:
            material->setParam(name, luax::checkVector3(L, 3));
            break;
        case 4:
            material->setParam(name, luax::checkVector4(L, 3));
            break;
        }
        return 0;
    }
    material->setParam(name, (float)luaL_checknumber(L, 3));
    return 0;
}
// 0x08149c60
static int Material_getName(lua_State* L)
{
    lua_pushstring(L, luax::checkObject<Material>(L, 1)->getName().c_str());
    return 1;
}
// 0x08149630
static int Material_getDiffuseMap(lua_State* L)
{
    pushSharedObject<RenderableTexture>(L, luax::checkObject<Material>(L, 1)->getDiffuseMap());
    return 1;
}
// 0x0814a3a0
static int Material_getSpecularMap(lua_State* L)
{
    pushSharedObject<RenderableTexture>(L, luax::checkObject<Material>(L, 1)->getSpecularMap());
    return 1;
}
// 0x0814a330
static int Material_getNormalMap(lua_State* L)
{
    pushSharedObject<RenderableTexture>(L, luax::checkObject<Material>(L, 1)->getNormalMap());
    return 1;
}
// 0x081496a0
static int Material_getShader(lua_State* L)
{
    pushSharedObject<RenderableShader>(L, luax::checkObject<Material>(L, 1)->getShader());
    return 1;
}
// 0x08149e60
static int Material_getDoubleSided(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<Material>(L, 1)->getDoubleSided());
    return 1;
}
// 0x08149df0
static int Material_getLighting(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<Material>(L, 1)->getLighting());
    return 1;
}
// 0x08149d80
static int Material_getAlphaTest(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<Material>(L, 1)->getAlphaTest());
    return 1;
}
// 0x0814a610
static int Material_getBlendMode(lua_State* L)
{
    pushEnumName(L, g_blendModes, luax::checkObject<Material>(L, 1)->getBlendMode());
    return 1;
}
// 0x0814a560
static int Material_getTextureAddressMode(lua_State* L)
{
    pushEnumName(L, g_textureAddressModes,
                 luax::checkObject<Material>(L, 1)->getTextureAddressMode());
    return 1;
}
// 0x0814a4f0
static int Material_getGlossiness(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<Material>(L, 1)->getGlossiness());
    return 1;
}
// 0x0814a480
static int Material_getDepthBias(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<Material>(L, 1)->getDepthBias());
    return 1;
}
// 0x08143520
static int Material_findMaterialByName(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    pushSharedObject<Material>(L, Material::findMaterialByName(name));
    return 1;
}
// 0x081434d0
static int Material_getMaterialByName(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    pushSharedObject<Material>(L, Material::getMaterialByName(name));
    return 1;
}
#if GRIMROCK_GAME < 2
// 0x0814a410
static int Material_activate(lua_State* L)
{
    Material* material = luax::checkObject<Material>(L, 1);
    Graphics::sm_pActive->activateMaterial(*material);
    return 0;
}
#endif

#if GRIMROCK_GAME >= 2
const luaL_Reg Material_methods[] = {{"create", Material_create},
                                     {"clone", Material_clone},
                                     {"setName", Material_setName},
                                     {"setDiffuseMap", Material_setDiffuseMap},
                                     {"setSpecularMap", Material_setSpecularMap},
                                     {"setNormalMap", Material_setNormalMap},
                                     {"setEmissiveMap", Material_setEmissiveMap},
                                     {"setShader", Material_setShader},
                                     {"setDoubleSided", Material_setDoubleSided},
                                     {"setLighting", Material_setLighting},
                                     {"setAmbientOcclusion", Material_setAmbientOcclusion},
                                     {"setCastShadow", Material_setCastShadow},
                                     {"setAlphaTest", Material_setAlphaTest},
                                     {"setBlendMode", Material_setBlendMode},
                                     {"setTextureAddressMode", Material_setTextureAddressMode},
                                     {"setTexcoordScaleOffset", Material_setTexcoordScaleOffset},
                                     {"setGlossiness", Material_setGlossiness},
                                     {"setDepthBias", Material_setDepthBias},
                                     {"setTexture", Material_setTexture},
                                     {"setTextureFilter", Material_setTextureFilter},
                                     {"setTextureAddress", Material_setTextureAddress},
                                     {"setParam", Material_setParam},
                                     {"getName", Material_getName},
                                     {"getDiffuseMap", Material_getDiffuseMap},
                                     {"getSpecularMap", Material_getSpecularMap},
                                     {"getNormalMap", Material_getNormalMap},
                                     {"getEmissiveMap", Material_getEmissiveMap},
                                     {"getShader", Material_getShader},
                                     {"getDoubleSided", Material_getDoubleSided},
                                     {"getLighting", Material_getLighting},
                                     {"getAmbientOcclusion", Material_getAmbientOcclusion},
                                     {"getCastShadow", Material_getCastShadow},
                                     {"getAlphaTest", Material_getAlphaTest},
                                     {"getBlendMode", Material_getBlendMode},
                                     {"getTextureAddressMode", Material_getTextureAddressMode},
                                     {"getTexcoordScaleOffset", Material_getTexcoordScaleOffset},
                                     {"getGlossiness", Material_getGlossiness},
                                     {"getDepthBias", Material_getDepthBias},
                                     {"findMaterialByName", Material_findMaterialByName},
                                     {"getMaterialByName", Material_getMaterialByName},
                                     {0, 0}};
const char* Material_properties[] = {"Name",       "DiffuseMap",         "SpecularMap",
                                     "NormalMap",  "Shader",             "DoubleSided",
                                     "Lighting",   "CastShadow",         "AlphaTest",
                                     "BlendMode",  "TextureAddressMode", "Glossiness",
                                     "DepthBias",  0};
#else
const luaL_Reg Material_methods[] = {{"create", Material_create},
                                     {"setName", Material_setName},
                                     {"setDiffuseMap", Material_setDiffuseMap},
                                     {"setSpecularMap", Material_setSpecularMap},
                                     {"setNormalMap", Material_setNormalMap},
                                     {"setShader", Material_setShader},
                                     {"setDoubleSided", Material_setDoubleSided},
                                     {"setLighting", Material_setLighting},
                                     {"setAlphaTest", Material_setAlphaTest},
                                     {"setBlendMode", Material_setBlendMode},
                                     {"setTextureAddressMode", Material_setTextureAddressMode},
                                     {"setGlossiness", Material_setGlossiness},
                                     {"setDepthBias", Material_setDepthBias},
                                     {"setTexture", Material_setTexture},
                                     {"setTextureFilter", Material_setTextureFilter},
                                     {"setTextureAddress", Material_setTextureAddress},
                                     {"setParam", Material_setParam},
                                     {"getName", Material_getName},
                                     {"getDiffuseMap", Material_getDiffuseMap},
                                     {"getSpecularMap", Material_getSpecularMap},
                                     {"getNormalMap", Material_getNormalMap},
                                     {"getShader", Material_getShader},
                                     {"getDoubleSided", Material_getDoubleSided},
                                     {"getLighting", Material_getLighting},
                                     {"getAlphaTest", Material_getAlphaTest},
                                     {"getBlendMode", Material_getBlendMode},
                                     {"getTextureAddressMode", Material_getTextureAddressMode},
                                     {"getGlossiness", Material_getGlossiness},
                                     {"getDepthBias", Material_getDepthBias},
                                     {"findMaterialByName", Material_findMaterialByName},
                                     {"getMaterialByName", Material_getMaterialByName},
                                     {"activate", Material_activate},
                                     {0, 0}};
const char* Material_properties[] = {
    "Name",     "DiffuseMap", "SpecularMap", "NormalMap",          "Shader",     "DoubleSided",
    "Lighting", "AlphaTest",  "BlendMode",   "TextureAddressMode", "Glossiness", "DepthBias",
    0};
#endif

#if GRIMROCK_GAME < 2
// ---- MaterialLibrary -------------------------------------------------------------

// 0x08144a60: MaterialLibrary.load(filename [, flags])
static int MaterialLibrary_load(lua_State* L)
{
    try
    {
        const char* filename = luaL_checkstring(L, 1);
        int flags = 0;
        if (lua_gettop(L) > 1)
            flags = luaL_checkinteger(L, 2);
        MaterialLibrary* library = loadMaterialLibrary(filename, flags);
        luax::createSharedObject<MaterialLibrary>(L, library);
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x0813cf60
static int MaterialLibrary_getMaterialCount(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<MaterialLibrary>(L, 1)->getMaterialCount());
    return 1;
}
// 0x08143300
static int MaterialLibrary_getMaterial(lua_State* L)
{
    MaterialLibrary* library = luax::checkObject<MaterialLibrary>(L, 1);
    int index = luaL_checkinteger(L, 2);
    if (index > 0 && index <= library->getMaterialCount())
    {
        pushSharedObject<Material>(L, library->getMaterial(index - 1));
        return 1;
    }
    return luaL_argerror(L, 2, "material index out of range");
}
// 0x08143570
static int MaterialLibrary_findMaterial(lua_State* L)
{
    MaterialLibrary* library = luax::checkObject<MaterialLibrary>(L, 1);
    const char* name = luaL_checkstring(L, 2);
    pushSharedObject<Material>(L, library->findMaterial(name));
    return 1;
}

const luaL_Reg MaterialLibrary_methods[] = {{"load", MaterialLibrary_load},
                                            {"getMaterialCount", MaterialLibrary_getMaterialCount},
                                            {"getMaterial", MaterialLibrary_getMaterial},
                                            {"findMaterial", MaterialLibrary_findMaterial},
                                            {0, 0}};

#endif

// ---- Font ------------------------------------------------------------------------

// 0x08144510: Font.create("fixedsys" | "consolas")
static int Font_create(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    int builtin = 0;
    if (strcmp(name, "fixedsys") != 0)
    {
        if (strcmp(name, "consolas") == 0)
        {
            builtin = 1;
        }
        else
        {
            builtin = 0;
            luaL_error(L, "unknown built-in font");
        }
    }
    luax::createSharedObject<Font>(L, new Font(builtin));
    return 1;
}
// 0x08146df0: Font.load(filename [, charWidth [, spacing]])
static int Font_load(lua_State* L)
{
    try
    {
        const char* filename = luaL_checkstring(L, 1);
        int charWidth = luaL_optinteger(L, 2, -1);
        int spacing = luaL_optinteger(L, 3, 0);
        luax::createSharedObject<Font>(L, new Font(filename, charWidth, spacing));
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x08146c70: Font.loadTrueType(filename, size [, "stroke"])
static int Font_loadTrueType(lua_State* L)
{
    try
    {
        luax::Enum styles[] = {{"stroke", 1}, {0, 0}};
        const char* filename = luaL_checkstring(L, 1);
        int size = luaL_checkinteger(L, 2);
        int style = 0;
        if (lua_gettop(L) > 2)
            style = luax::checkEnum(L, 3, styles);
        Font* font = Font::loadTrueType(filename, size, style);
        luax::createSharedObject<Font>(L, font);
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x0813a9f0
static int Font_getLineHeight(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<Font>(L, 1)->getLineHeight());
    return 1;
}
// 0x0813d910
static int Font_getMaxBearingY(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<Font>(L, 1)->getAscent());
    return 1;
}
// 0x0813d980
static int Font_getTextWidth(lua_State* L)
{
    Font* font = luax::checkObject<Font>(L, 1);
    const char* text = luaL_checkstring(L, 2);
    lua_pushnumber(L, font->getWidth(text));
    return 1;
}
// 0x0813db30
static int Font_isPrintable(lua_State* L)
{
    Font* font = luax::checkObject<Font>(L, 1);
    const char* ch = luaL_checkstring(L, 2);
    if (strlen(ch) != 1)
        return luaL_error(L, "invalid character");
    lua_pushboolean(L, font->isPrintable((unsigned char)ch[0]));
    return 1;
}

const luaL_Reg Font_methods[] = {{"create", Font_create},
                                 {"load", Font_load},
                                 {"loadTrueType", Font_loadTrueType},
                                 {"getLineHeight", Font_getLineHeight},
                                 {"getMaxBearingY", Font_getMaxBearingY},
                                 {"getTextWidth", Font_getTextWidth},
                                 {"isPrintable", Font_isPrintable},
                                 {0, 0}};
