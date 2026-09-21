// RenderEntity, MeshEntity, LightEntity and particle system bindings of Engine.cpp.
#include "EngineBindings.h"
#include "core/Exception.h"

using namespace core;
using namespace engine;

// 0x082be7a8
luax::Enum g_particleEmitterBlendModes[] = {{"Translucent", 0}, {"Additive", 1}, {0, 0}};

// ---- RenderEntity ----------------------------------------------------------------

// 0x0814d990
static int RenderEntity_setDrawBoundBox(lua_State* L)
{
    RenderEntity* entity = luax::checkObject<RenderEntity>(L, 1);
    entity->setDrawBoundBox(luax::checkBool(L, 2));
    return 0;
}
// 0x0814d910
static int RenderEntity_setHidden(lua_State* L)
{
    RenderEntity* entity = luax::checkObject<RenderEntity>(L, 1);
    entity->setHidden(luax::checkBool(L, 2));
    return 0;
}
// 0x0813f320
static int RenderEntity_setSortOffset(lua_State* L)
{
    RenderEntity* entity = luax::checkObject<RenderEntity>(L, 1);
    entity->setSortOffset((float)luaL_checknumber(L, 2));
    return 0;
}
// 0x0813f160
static int RenderEntity_getEntityType(lua_State* L)
{
    static const luax::Enum types[] = {
        {"MeshEntity", 0}, {"LightEntity", 1}, {"ParticleEntity", 2}, {0, 0}};
    RenderEntity* entity = luax::checkObject<RenderEntity>(L, 1);
    for (const luax::Enum* e = types; e->name; ++e)
    {
        if (e->value == entity->getEntityType())
        {
            lua_pushstring(L, e->name);
            return 1;
        }
    }
    lua_pushstring(L, "???");
    return 1;
}
// 0x0813f2b0
static int RenderEntity_getDrawBoundBox(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<RenderEntity>(L, 1)->getDrawBoundBox());
    return 1;
}
// 0x0813f240
static int RenderEntity_getHidden(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<RenderEntity>(L, 1)->getHidden());
    return 1;
}
// 0x0813f0f0
static int RenderEntity_getSortOffset(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<RenderEntity>(L, 1)->getSortOffset());
    return 1;
}
// 0x0813f390
static int RenderEntity_getNode(lua_State* L)
{
    RenderEntity* entity = luax::checkObject<RenderEntity>(L, 1);
    luax::pushObject(L, entity->getNode());
    return 1;
}
// 0x08142c90
static int RenderEntity_getWorldBounds(lua_State* L)
{
    RenderEntity* entity = luax::checkObject<RenderEntity>(L, 1);
    if (entity->isUnbounded())
        luaL_error(L, "unbounded entity");
    luax::pushBox(L, entity->getWorldBounds());
    return 1;
}

const luaL_Reg RenderEntity_methods[] = {{"setDrawBoundBox", RenderEntity_setDrawBoundBox},
                                         {"setHidden", RenderEntity_setHidden},
                                         {"setSortOffset", RenderEntity_setSortOffset},
                                         {"getEntityType", RenderEntity_getEntityType},
                                         {"getDrawBoundBox", RenderEntity_getDrawBoundBox},
                                         {"getHidden", RenderEntity_getHidden},
                                         {"getSortOffset", RenderEntity_getSortOffset},
                                         {"getNode", RenderEntity_getNode},
                                         {"getWorldBounds", RenderEntity_getWorldBounds},
                                         {0, 0}};
const char* RenderEntity_properties[] = {"DrawBoundBox", "Hidden", "SortOffset", 0};

// ---- MeshEntity ------------------------------------------------------------------

// 0x08147fc0: MeshEntity.create([mesh | renderableMesh])
static int MeshEntity_create(lua_State* L)
{
    MeshEntity* entity;
    if (lua_gettop(L) == 0)
    {
        entity = new MeshEntity(0);
    }
    else
    {
        Mesh* mesh = luax::checkObjectOpt<Mesh>(L, 1);
        if (!mesh)
        {
            RenderableMesh* rmesh = luax::checkObjectOpt<RenderableMesh>(L, 1);
            if (!rmesh)
                luaL_typerror(L, 1, "Mesh"); // the original returns the argument itself
            entity = new MeshEntity(rmesh);
            luax::createSharedObject<MeshEntity>(L, entity);
            return 1;
        }
        if (mesh->getIndices().size() == 0)
            luaL_error(L, "mesh does not have indices");
        RenderableMesh* rmesh = createRenderableMesh(*mesh, false);
        entity = new MeshEntity(rmesh);
    }
    luax::createSharedObject<MeshEntity>(L, entity);
    return 1;
}
// 0x08141400
static int MeshEntity_setMesh(lua_State* L)
{
    MeshEntity* entity = luax::checkObject<MeshEntity>(L, 1);
    if (lua_type(L, 2) != LUA_TNIL)
    {
        entity->setMesh(luax::checkObject<RenderableMesh>(L, 2));
        return 0;
    }
    entity->setMesh(0);
    return 0;
}
// 0x08141720
static int MeshEntity_setSkeleton(lua_State* L)
{
    MeshEntity* entity = luax::checkObject<MeshEntity>(L, 1);
    if (lua_type(L, 2) != LUA_TNIL)
    {
        entity->setSkeleton(luax::checkObject<Skeleton>(L, 2));
        return 0;
    }
    entity->setSkeleton(0);
    return 0;
}
// 0x08149330
static int MeshEntity_setMaterial(lua_State* L)
{
    MeshEntity* entity = luax::checkObject<MeshEntity>(L, 1);
    Material* material = luax::checkObject<Material>(L, 2);
    entity->setMaterial(material);
    return 0;
}
// 0x08147ac0
static int MeshEntity_setMaterials(lua_State* L)
{
    MeshEntity* entity = luax::checkObject<MeshEntity>(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    int n = (int)lua_objlen(L, 2);
    Array<SharedPtr<Material>> materials;
    for (int i = 0; i < n; ++i)
    {
        lua_rawgeti(L, 2, i + 1);
        Material* material = luax::checkObject<Material>(L, lua_gettop(L));
        materials.push_back(SharedPtr<Material>(material));
        lua_pop(L, 1);
    }
    entity->setMaterials(materials);
    return 0;
}
// 0x081416a0
static int MeshEntity_setEmissiveColor(lua_State* L)
{
    MeshEntity* entity = luax::checkObject<MeshEntity>(L, 1);
    entity->setEmissiveColor(luax::checkVector3_alt(L, 2));
    return 0;
}
// 0x0814dbc0
static int MeshEntity_setCastShadow(lua_State* L)
{
    MeshEntity* entity = luax::checkObject<MeshEntity>(L, 1);
    entity->setFlag(MeshEntity::CastShadow, luax::checkBool(L, 2));
    return 0;
}
// 0x0814db30
static int MeshEntity_setStaticShadow(lua_State* L)
{
    MeshEntity* entity = luax::checkObject<MeshEntity>(L, 1);
    entity->setFlag(MeshEntity::StaticShadow, luax::checkBool(L, 2));
    return 0;
}
// 0x0814daa0
static int MeshEntity_setDrawNormals(lua_State* L)
{
    MeshEntity* entity = luax::checkObject<MeshEntity>(L, 1);
    entity->setFlag(MeshEntity::DrawNormals, luax::checkBool(L, 2));
    return 0;
}
// 0x0814da10
static int MeshEntity_setDrawTangents(lua_State* L)
{
    MeshEntity* entity = luax::checkObject<MeshEntity>(L, 1);
    entity->setFlag(MeshEntity::DrawTangents, luax::checkBool(L, 2));
    return 0;
}
// 0x081472b0
static int MeshEntity_setManualBounds(lua_State* L)
{
    MeshEntity* entity = luax::checkObject<MeshEntity>(L, 1);
    entity->setManualBounds(luax::checkBox(L, 2));
    return 0;
}
// 0x081435f0
static int MeshEntity_getMesh(lua_State* L)
{
    MeshEntity* entity = luax::checkObject<MeshEntity>(L, 1);
    pushSharedObject<RenderableMesh>(L, entity->getMesh());
    return 1;
}
// 0x08145b30
static int MeshEntity_getSkeleton(lua_State* L)
{
    MeshEntity* entity = luax::checkObject<MeshEntity>(L, 1);
    pushSharedObject<Skeleton>(L, entity->getSkeleton());
    return 1;
}
// 0x08143260
static int MeshEntity_getMaterials(lua_State* L)
{
    MeshEntity* entity = luax::checkObject<MeshEntity>(L, 1);
    const Array<SharedPtr<Material>>& materials = entity->getMaterials();
    lua_createtable(L, 0, 0);
    for (int i = 0; i < materials.size(); ++i)
    {
        pushSharedObject<Material>(L, materials[i].get());
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}
// 0x08141620
static int MeshEntity_getEmissiveColor(lua_State* L)
{
    luax::pushVector(L, luax::checkObject<MeshEntity>(L, 1)->getEmissiveColor());
    return 1;
}
// 0x081415b0
static int MeshEntity_getCastShadow(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<MeshEntity>(L, 1)->getFlag(MeshEntity::CastShadow));
    return 1;
}
// 0x08141540
static int MeshEntity_getStaticShadow(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<MeshEntity>(L, 1)->getFlag(MeshEntity::StaticShadow));
    return 1;
}
// 0x081414d0
static int MeshEntity_getDrawNormals(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<MeshEntity>(L, 1)->getFlag(MeshEntity::DrawNormals));
    return 1;
}
// 0x0813af30
static int MeshEntity_getDrawTangents(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<MeshEntity>(L, 1)->getFlag(MeshEntity::DrawTangents));
    return 1;
}

const luaL_Reg MeshEntity_methods[] = {{"create", MeshEntity_create},
                                       {"setMesh", MeshEntity_setMesh},
                                       {"setSkeleton", MeshEntity_setSkeleton},
                                       {"setMaterial", MeshEntity_setMaterial},
                                       {"setMaterials", MeshEntity_setMaterials},
                                       {"setEmissiveColor", MeshEntity_setEmissiveColor},
                                       {"setCastShadow", MeshEntity_setCastShadow},
                                       {"setStaticShadow", MeshEntity_setStaticShadow},
                                       {"setDrawNormals", MeshEntity_setDrawNormals},
                                       {"setDrawTangents", MeshEntity_setDrawTangents},
                                       {"setManualBounds", MeshEntity_setManualBounds},
                                       {"getMesh", MeshEntity_getMesh},
                                       {"getSkeleton", MeshEntity_getSkeleton},
                                       {"getMaterials", MeshEntity_getMaterials},
                                       {"getEmissiveColor", MeshEntity_getEmissiveColor},
                                       {"getCastShadow", MeshEntity_getCastShadow},
                                       {"getStaticShadow", MeshEntity_getStaticShadow},
                                       {"getDrawNormals", MeshEntity_getDrawNormals},
                                       {"getDrawTangents", MeshEntity_getDrawTangents},
                                       {0, 0}};

// ---- LightEntity -----------------------------------------------------------------

// 0x08145d60: LightEntity.create(type)
static int LightEntity_create(lua_State* L)
{
    int type = luax::checkEnum(L, 1, g_lightTypes);
    LightEntity* light = new LightEntity((LightEntity::LightType)type);
    luax::createSharedObject<LightEntity>(L, light);
    return 1;
}
// 0x081427b0
static int LightEntity_setLightColor(lua_State* L)
{
    LightEntity* light = luax::checkObject<LightEntity>(L, 1);
    light->setLightColor(luax::checkVector3_alt(L, 2));
    return 0;
}
// 0x08142910
static int LightEntity_setLightRange(lua_State* L)
{
    LightEntity* light = luax::checkObject<LightEntity>(L, 1);
    light->setLightRange((float)luaL_checknumber(L, 2));
    return 0;
}
// 0x08142980
static int LightEntity_setSpotAngle(lua_State* L)
{
    LightEntity* light = luax::checkObject<LightEntity>(L, 1);
    light->setSpotAngle((float)luaL_checknumber(L, 2));
    return 0;
}
// 0x081428a0
static int LightEntity_setSpotSharpness(lua_State* L)
{
    LightEntity* light = luax::checkObject<LightEntity>(L, 1);
    light->setSpotSharpness((float)luaL_checknumber(L, 2));
    return 0;
}
// 0x0814dcd0
static int LightEntity_setCastShadow(lua_State* L)
{
    LightEntity* light = luax::checkObject<LightEntity>(L, 1);
    light->setCastShadow(luax::checkBool(L, 2));
    return 0;
}
// 0x08142830
static int LightEntity_setMaxShadowDistance(lua_State* L)
{
    LightEntity* light = luax::checkObject<LightEntity>(L, 1);
    light->setMaxShadowDistance((float)luaL_checknumber(L, 2));
    return 0;
}
// 0x081426c0
static int LightEntity_setShadowMapSize(lua_State* L)
{
    LightEntity* light = luax::checkObject<LightEntity>(L, 1);
    light->setShadowMapSize(luaL_checkinteger(L, 2));
    return 0;
}
// 0x081429f0
static int LightEntity_setStaticShadowMap(lua_State* L)
{
    LightEntity* light = luax::checkObject<LightEntity>(L, 1);
    if (lua_type(L, 2) == LUA_TNIL)
    {
        light->setStaticShadowMap(0);
        return 0;
    }
    light->setStaticShadowMap(luax::checkObject<RenderableTexture>(L, 2));
    return 0;
}
// 0x0814dc50
static int LightEntity_setPrimaryLight(lua_State* L)
{
    LightEntity* light = luax::checkObject<LightEntity>(L, 1);
    light->setPrimaryLight(luax::checkBool(L, 2));
    return 0;
}
// 0x08142730
static int LightEntity_getLightColor(lua_State* L)
{
    luax::pushVector(L, luax::checkObject<LightEntity>(L, 1)->getLightColor());
    return 1;
}
// 0x08142c20
static int LightEntity_getLightRange(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<LightEntity>(L, 1)->getLightRange());
    return 1;
}
// 0x08142bb0
static int LightEntity_getSpotAngle(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<LightEntity>(L, 1)->getSpotAngle());
    return 1;
}
// 0x08142b40
static int LightEntity_getSpotSharpness(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<LightEntity>(L, 1)->getSpotSharpness());
    return 1;
}
// 0x08142650
static int LightEntity_getCastShadow(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<LightEntity>(L, 1)->getCastShadow());
    return 1;
}
// 0x08142ad0
static int LightEntity_getMaxShadowDistance(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<LightEntity>(L, 1)->getMaxShadowDistance());
    return 1;
}
// 0x0813a680
static int LightEntity_getShadowMapSize(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<LightEntity>(L, 1)->getShadowMapSize());
    return 1;
}
// 0x08143760
static int LightEntity_getStaticShadowMap(lua_State* L)
{
    LightEntity* light = luax::checkObject<LightEntity>(L, 1);
    pushSharedObject<RenderableTexture>(L, light->getStaticShadowMap());
    return 1;
}
// 0x081425e0
static int LightEntity_getPrimaryLight(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<LightEntity>(L, 1)->getPrimaryLight());
    return 1;
}
// 0x081478d0: renderStaticShadowMap(size, bias, flags) -> texture
static int LightEntity_renderStaticShadowMap(lua_State* L)
{
    try
    {
        LightEntity* light = luax::checkObject<LightEntity>(L, 1);
        unsigned size = luaL_checkinteger(L, 2);
        float bias = (float)luaL_checknumber(L, 3);
        int flags = luaL_checkinteger(L, 4);
        constexpr unsigned MaxShadowMapSize = 2048;
        if (size > MaxShadowMapSize || size == 0 || ((size - 1) & size) != 0)
            luaL_error(L, "invalid shadow map size");
        RenderableTexture* texture = light->renderStaticShadowMap(size, bias, flags);
        if (!texture)
        {
            lua_pushnil(L);
            return 1;
        }
        luax::createSharedObject<RenderableTexture>(L, texture);
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}

const luaL_Reg LightEntity_methods[] = {
    {"create", LightEntity_create},
    {"setLightColor", LightEntity_setLightColor},
    {"setLightRange", LightEntity_setLightRange},
    {"setSpotAngle", LightEntity_setSpotAngle},
    {"setSpotSharpness", LightEntity_setSpotSharpness},
    {"setCastShadow", LightEntity_setCastShadow},
    {"setMaxShadowDistance", LightEntity_setMaxShadowDistance},
    {"setShadowMapSize", LightEntity_setShadowMapSize},
    {"setStaticShadowMap", LightEntity_setStaticShadowMap},
    {"setPrimaryLight", LightEntity_setPrimaryLight},
    {"getLightColor", LightEntity_getLightColor},
    {"getLightRange", LightEntity_getLightRange},
    {"getSpotAngle", LightEntity_getSpotAngle},
    {"getSpotSharpness", LightEntity_getSpotSharpness},
    {"getCastShadow", LightEntity_getCastShadow},
    {"getMaxShadowDistance", LightEntity_getMaxShadowDistance},
    {"getShadowMapSize", LightEntity_getShadowMapSize},
    {"getStaticShadowMap", LightEntity_getStaticShadowMap},
    {"getPrimaryLight", LightEntity_getPrimaryLight},
    {"renderStaticShadowMap", LightEntity_renderStaticShadowMap},
    {0, 0}};
const char* LightEntity_properties[] = {"LightColor",    "LightRange",   "SpotAngle",
                                        "SpotSharpness", "CastShadow",   "MaxShadowDistance",
                                        "ShadowMapSize", "PrimaryLight", 0};

// ---- ParticleEntity --------------------------------------------------------------

// 0x081457b0: ParticleEntity.create([system])
static int ParticleEntity_create(lua_State* L)
{
    ParticleSystem* system = 0;
    if (lua_gettop(L) > 0)
        system = luax::checkObject<ParticleSystem>(L, 1);
    ParticleEntity* entity = new ParticleEntity(system);
    luax::createSharedObject<ParticleEntity>(L, entity);
    return 1;
}
// 0x0813ded0
static int ParticleEntity_setParticleSystem(lua_State* L)
{
    ParticleEntity* entity = luax::checkObject<ParticleEntity>(L, 1);
    ParticleSystem* system = luax::checkObject<ParticleSystem>(L, 2);
    entity->setParticleSystem(system);
    return 0;
}
// 0x0813e1b0
static int ParticleEntity_isAlive(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<ParticleEntity>(L, 1)->isAlive());
    return 1;
}
// 0x0813e150
static int ParticleEntity_reset(lua_State* L)
{
    luax::checkObject<ParticleEntity>(L, 1)->reset();
    return 0;
}
// 0x0813e0f0
static int ParticleEntity_start(lua_State* L)
{
    luax::checkObject<ParticleEntity>(L, 1)->start();
    return 0;
}
// 0x0813e090
static int ParticleEntity_stop(lua_State* L)
{
    luax::checkObject<ParticleEntity>(L, 1)->stop();
    return 0;
}
// 0x0813e000
static int ParticleEntity_update(lua_State* L)
{
    ParticleEntity* entity = luax::checkObject<ParticleEntity>(L, 1);
    float dt = (float)luaL_checknumber(L, 2);
    if (!entity->getNode())
        luaL_error(L, "particle system is not attached to a node");
    entity->update(dt);
    return 0;
}

const luaL_Reg ParticleEntity_methods[] = {
    {"create", ParticleEntity_create},   {"setParticleSystem", ParticleEntity_setParticleSystem},
    {"isAlive", ParticleEntity_isAlive}, {"reset", ParticleEntity_reset},
    {"start", ParticleEntity_start},     {"stop", ParticleEntity_stop},
    {"update", ParticleEntity_update},   {0, 0}};

// ---- ParticleSystem --------------------------------------------------------------

// 0x08147550
static int ParticleSystem_create(lua_State* L)
{
    luax::createSharedObject<ParticleSystem>(L, new ParticleSystem());
    return 1;
}
// 0x08147430
static int ParticleSystem_load(lua_State* L)
{
    try
    {
        const char* filename = luaL_checkstring(L, 1);
        luax::createSharedObject<ParticleSystem>(L, loadParticleSystem(filename));
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x0813de20
static int ParticleSystem_reload(lua_State* L)
{
    try
    {
        ParticleSystem* system = luax::checkObject<ParticleSystem>(L, 1);
        system->load(system->getFilename().c_str());
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x08144680
static int ParticleSystem_getParticleSystemByFilename(lua_State* L)
{
    try
    {
        const char* filename = luaL_checkstring(L, 1);
        pushSharedObject<ParticleSystem>(L, ParticleSystem::getParticleSystemByFilename(filename));
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x0813ddb0
static int ParticleSystem_addParticleEmitter(lua_State* L)
{
    ParticleSystem* system = luax::checkObject<ParticleSystem>(L, 1);
    ParticleEmitter* emitter = luax::checkObject<ParticleEmitter>(L, 2);
    system->addParticleEmitter(emitter);
    return 0;
}
// 0x0813dd40
static int ParticleSystem_removeParticleEmitter(lua_State* L)
{
    ParticleSystem* system = luax::checkObject<ParticleSystem>(L, 1);
    ParticleEmitter* emitter = luax::checkObject<ParticleEmitter>(L, 2);
    system->removeParticleEmitter(emitter);
    return 0;
}

const luaL_Reg ParticleSystem_methods[] = {
    {"create", ParticleSystem_create},
    {"load", ParticleSystem_load},
    {"reload", ParticleSystem_reload},
    {"getParticleSystemByFilename", ParticleSystem_getParticleSystemByFilename},
    {"addParticleEmitter", ParticleSystem_addParticleEmitter},
    {"removeParticleEmitter", ParticleSystem_removeParticleEmitter},
    {0, 0}};

// ---- ParticleEmitter -------------------------------------------------------------

// {min, max} pairs
static void checkRange(lua_State* L, int index, float& lo, float& hi)
{
    if (lua_type(L, index) != LUA_TTABLE)
        luaL_typerror(L, index, "table");
    lua_rawgeti(L, index, 1);
    lo = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);
    lua_rawgeti(L, index, 2);
    hi = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);
}
static void pushRange(lua_State* L, float lo, float hi)
{
    lua_createtable(L, 0, 0);
    lua_pushnumber(L, lo);
    lua_rawseti(L, -2, 1);
    lua_pushnumber(L, hi);
    lua_rawseti(L, -2, 2);
}

// 0x08144970
static int ParticleEmitter_create(lua_State* L)
{
    luax::createSharedObject<ParticleEmitter>(L, new ParticleEmitter());
    return 1;
}
// 0x0813ba60
static int ParticleEmitter_setEmissionRate(lua_State* L)
{
    luax::checkObject<ParticleEmitter>(L, 1)->m_emissionRate = (float)luaL_checknumber(L, 2);
    return 0;
}
// 0x0813ba20
static int ParticleEmitter_setEmissionTime(lua_State* L)
{
    luax::checkObject<ParticleEmitter>(L, 1)->m_emissionTime = (float)luaL_checknumber(L, 2);
    return 0;
}
// 0x0813b2e0
static int ParticleEmitter_setMaxParticles(lua_State* L)
{
    luax::checkObject<ParticleEmitter>(L, 1)->m_maxParticles = luaL_checkinteger(L, 2);
    return 0;
}
// 0x0814de70
static int ParticleEmitter_setSpawnBurst(lua_State* L)
{
    ParticleEmitter* emitter = luax::checkObject<ParticleEmitter>(L, 1);
    emitter->m_spawnBurst = luax::checkBool(L, 2);
    return 0;
}
// 0x0814cdd0
static int ParticleEmitter_setSprayAngle(lua_State* L)
{
    ParticleEmitter* emitter = luax::checkObject<ParticleEmitter>(L, 1);
    checkRange(L, 2, emitter->m_sprayAngleMin, emitter->m_sprayAngleMax);
    return 0;
}
// 0x0813b7a0
static int ParticleEmitter_setBoxMin(lua_State* L)
{
    luax::checkObject<ParticleEmitter>(L, 1)->m_boxMin = luax::checkVector3_alt(L, 2);
    return 0;
}
// 0x0813b740
static int ParticleEmitter_setBoxMax(lua_State* L)
{
    luax::checkObject<ParticleEmitter>(L, 1)->m_boxMax = luax::checkVector3_alt(L, 2);
    return 0;
}
// 0x0813bde0
static int ParticleEmitter_setMesh(lua_State* L)
{
    ParticleEmitter* emitter = luax::checkObject<ParticleEmitter>(L, 1);
    if (lua_type(L, 2) != LUA_TNIL)
    {
        emitter->setMesh(luax::checkObject<MeshCDF>(L, 2));
        return 0;
    }
    emitter->setMesh(0);
    return 0;
}
// 0x0813bd30
static int ParticleEmitter_setSkeleton(lua_State* L)
{
    ParticleEmitter* emitter = luax::checkObject<ParticleEmitter>(L, 1);
    if (lua_type(L, 2) == LUA_TNIL)
    {
        emitter->setSkeleton(0);
        return 0;
    }
    emitter->setSkeleton(luax::checkObject<Skeleton>(L, 2));
    return 0;
}
// 0x0814ccf0
static int ParticleEmitter_setVelocity(lua_State* L)
{
    ParticleEmitter* emitter = luax::checkObject<ParticleEmitter>(L, 1);
    checkRange(L, 2, emitter->m_velocityMin, emitter->m_velocityMax);
    return 0;
}
// 0x08144ee0
static int ParticleEmitter_setTexture(lua_State* L)
{
    ParticleEmitter* emitter = luax::checkObject<ParticleEmitter>(L, 1);
    if (lua_type(L, 2) == LUA_TNIL)
    {
        emitter->m_texture.reset(0);
        return 0;
    }
    emitter->m_texture.reset(luax::checkObject<RenderableTexture>(L, 2));
    return 0;
}
// 0x0813b9e0
static int ParticleEmitter_setFrameRate(lua_State* L)
{
    luax::checkObject<ParticleEmitter>(L, 1)->m_frameRate = (float)luaL_checknumber(L, 2);
    return 0;
}
// 0x0813b2a0
static int ParticleEmitter_setFrameSize(lua_State* L)
{
    luax::checkObject<ParticleEmitter>(L, 1)->m_frameSize = luaL_checkinteger(L, 2);
    return 0;
}
// 0x0813b260
static int ParticleEmitter_setFrameCount(lua_State* L)
{
    luax::checkObject<ParticleEmitter>(L, 1)->m_frameCount = luaL_checkinteger(L, 2);
    return 0;
}
// 0x0814cc10
static int ParticleEmitter_setLifetime(lua_State* L)
{
    ParticleEmitter* emitter = luax::checkObject<ParticleEmitter>(L, 1);
    checkRange(L, 2, emitter->m_lifetimeMin, emitter->m_lifetimeMax);
    return 0;
}
// 0x0813b670: setColor(color) or setColor(index, color)
static int ParticleEmitter_setColor(lua_State* L)
{
    ParticleEmitter* emitter = luax::checkObject<ParticleEmitter>(L, 1);
    if (!lua_isnumber(L, 2))
    {
        emitter->m_color[0] = luax::checkVector3_alt(L, 2);
        return 0;
    }
    int index = luaL_checkinteger(L, 2);
    emitter->m_color[index] = luax::checkVector3_alt(L, 3);
    return 0;
}
// 0x0814de10
static int ParticleEmitter_setColorAnimation(lua_State* L)
{
    ParticleEmitter* emitter = luax::checkObject<ParticleEmitter>(L, 1);
    emitter->m_colorAnimation = luax::checkBool(L, 2);
    return 0;
}
// 0x0813b990
static int ParticleEmitter_setOpacity(lua_State* L)
{
    luax::checkObject<ParticleEmitter>(L, 1)->m_opacity = (float)luaL_checknumber(L, 2);
    return 0;
}
// 0x0813b940
static int ParticleEmitter_setFadeIn(lua_State* L)
{
    luax::checkObject<ParticleEmitter>(L, 1)->m_fadeIn = (float)luaL_checknumber(L, 2);
    return 0;
}
// 0x0813b8f0
static int ParticleEmitter_setFadeOut(lua_State* L)
{
    luax::checkObject<ParticleEmitter>(L, 1)->m_fadeOut = (float)luaL_checknumber(L, 2);
    return 0;
}
// 0x0814cb30
static int ParticleEmitter_setSize(lua_State* L)
{
    ParticleEmitter* emitter = luax::checkObject<ParticleEmitter>(L, 1);
    checkRange(L, 2, emitter->m_sizeMin, emitter->m_sizeMax);
    return 0;
}
// 0x0813b600
static int ParticleEmitter_setGravity(lua_State* L)
{
    luax::checkObject<ParticleEmitter>(L, 1)->m_gravity = luax::checkVector3_alt(L, 2);
    return 0;
}
// 0x0813b8a0
static int ParticleEmitter_setAirResistance(lua_State* L)
{
    luax::checkObject<ParticleEmitter>(L, 1)->m_airResistance = (float)luaL_checknumber(L, 2);
    return 0;
}
// 0x0813b850
static int ParticleEmitter_setRotationSpeed(lua_State* L)
{
    luax::checkObject<ParticleEmitter>(L, 1)->m_rotationSpeed = (float)luaL_checknumber(L, 2);
    return 0;
}
// 0x0813bce0
static int ParticleEmitter_setBlendMode(lua_State* L)
{
    ParticleEmitter* emitter = luax::checkObject<ParticleEmitter>(L, 1);
    emitter->m_blendMode = luax::checkEnum(L, 2, g_particleEmitterBlendModes);
    return 0;
}
// 0x0814ddb0
static int ParticleEmitter_setObjectSpace(lua_State* L)
{
    ParticleEmitter* emitter = luax::checkObject<ParticleEmitter>(L, 1);
    emitter->m_objectSpace = luax::checkBool(L, 2);
    return 0;
}
// 0x0814dd50
static int ParticleEmitter_setClampToGroundPlane(lua_State* L)
{
    ParticleEmitter* emitter = luax::checkObject<ParticleEmitter>(L, 1);
    emitter->m_clampToGroundPlane = luax::checkBool(L, 2);
    return 0;
}
// 0x0813b800
static int ParticleEmitter_setDepthBias(lua_State* L)
{
    luax::checkObject<ParticleEmitter>(L, 1)->m_depthBias = (float)luaL_checknumber(L, 2);
    return 0;
}
// 0x0813a9b0
static int ParticleEmitter_getEmissionRate(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<ParticleEmitter>(L, 1)->m_emissionRate);
    return 1;
}
// 0x0813a970
static int ParticleEmitter_getEmissionTime(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<ParticleEmitter>(L, 1)->m_emissionTime);
    return 1;
}
// 0x0813a930
static int ParticleEmitter_getMaxParticles(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<ParticleEmitter>(L, 1)->m_maxParticles);
    return 1;
}
// 0x0813b060
static int ParticleEmitter_getSpawnBurst(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<ParticleEmitter>(L, 1)->m_spawnBurst);
    return 1;
}
// 0x08142f40
static int ParticleEmitter_getSprayAngle(lua_State* L)
{
    ParticleEmitter* emitter = luax::checkObject<ParticleEmitter>(L, 1);
    pushRange(L, emitter->m_sprayAngleMin, emitter->m_sprayAngleMax);
    return 1;
}
// 0x0813d6c0
static int ParticleEmitter_getBoxMin(lua_State* L)
{
    luax::pushVector(L, luax::checkObject<ParticleEmitter>(L, 1)->m_boxMin);
    return 1;
}
// 0x0813d7a0
static int ParticleEmitter_getBoxMax(lua_State* L)
{
    luax::pushVector(L, luax::checkObject<ParticleEmitter>(L, 1)->m_boxMax);
    return 1;
}
// 0x08143860
static int ParticleEmitter_getMesh(lua_State* L)
{
    ParticleEmitter* emitter = luax::checkObject<ParticleEmitter>(L, 1);
    pushSharedObject<MeshCDF>(L, emitter->m_mesh.get());
    return 1;
}
// 0x08145ba0
static int ParticleEmitter_getSkeleton(lua_State* L)
{
    ParticleEmitter* emitter = luax::checkObject<ParticleEmitter>(L, 1);
    pushSharedObject<Skeleton>(L, emitter->m_skeleton.get());
    return 1;
}
// 0x0813b320
static int ParticleEmitter_getVelocity(lua_State* L)
{
    ParticleEmitter* emitter = luax::checkObject<ParticleEmitter>(L, 1);
    pushRange(L, emitter->m_velocityMin, emitter->m_velocityMax);
    return 1;
}
// 0x081437d0
static int ParticleEmitter_getTexture(lua_State* L)
{
    ParticleEmitter* emitter = luax::checkObject<ParticleEmitter>(L, 1);
    pushSharedObject<RenderableTexture>(L, emitter->m_texture.get());
    return 1;
}
// 0x0813a8f0
static int ParticleEmitter_getFrameRate(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<ParticleEmitter>(L, 1)->m_frameRate);
    return 1;
}
// 0x0813a8b0
static int ParticleEmitter_getFrameSize(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<ParticleEmitter>(L, 1)->m_frameSize);
    return 1;
}
// 0x0813a870
static int ParticleEmitter_getFrameCount(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<ParticleEmitter>(L, 1)->m_frameCount);
    return 1;
}
// 0x08143100
static int ParticleEmitter_getLifetime(lua_State* L)
{
    ParticleEmitter* emitter = luax::checkObject<ParticleEmitter>(L, 1);
    pushRange(L, emitter->m_lifetimeMin, emitter->m_lifetimeMax);
    return 1;
}
// 0x0814a7d0: getColor([index])
static int ParticleEmitter_getColor(lua_State* L)
{
    ParticleEmitter* emitter = luax::checkObject<ParticleEmitter>(L, 1);
    if (!lua_isnumber(L, 2))
    {
        luax::pushVector(L, emitter->m_color[0]);
        return 1;
    }
    int index = luaL_checkinteger(L, 2);
    luax::pushVector(L, emitter->m_color[index]);
    return 1;
}
// 0x0813b020
static int ParticleEmitter_getColorAnimation(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<ParticleEmitter>(L, 1)->m_colorAnimation);
    return 1;
}
// 0x0813a830
static int ParticleEmitter_getOpacity(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<ParticleEmitter>(L, 1)->m_opacity);
    return 1;
}
// 0x0813a7f0
static int ParticleEmitter_getFadeIn(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<ParticleEmitter>(L, 1)->m_fadeIn);
    return 1;
}
// 0x0813a7b0
static int ParticleEmitter_getFadeOut(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<ParticleEmitter>(L, 1)->m_fadeOut);
    return 1;
}
// 0x08143060
static int ParticleEmitter_getSize(lua_State* L)
{
    ParticleEmitter* emitter = luax::checkObject<ParticleEmitter>(L, 1);
    pushRange(L, emitter->m_sizeMin, emitter->m_sizeMax);
    return 1;
}
// 0x0813d660
static int ParticleEmitter_getGravity(lua_State* L)
{
    luax::pushVector(L, luax::checkObject<ParticleEmitter>(L, 1)->m_gravity);
    return 1;
}
// 0x0813a770
static int ParticleEmitter_getAirResistance(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<ParticleEmitter>(L, 1)->m_airResistance);
    return 1;
}
// 0x0813a730
static int ParticleEmitter_getRotationSpeed(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<ParticleEmitter>(L, 1)->m_rotationSpeed);
    return 1;
}
// 0x0813df70
static int ParticleEmitter_getBlendMode(lua_State* L)
{
    ParticleEmitter* emitter = luax::checkObject<ParticleEmitter>(L, 1);
    for (const luax::Enum* e = g_particleEmitterBlendModes; e->name; ++e)
    {
        if (e->value == emitter->m_blendMode)
        {
            lua_pushstring(L, e->name);
            return 1;
        }
    }
    lua_pushstring(L, "???");
    return 1;
}
// 0x0813afe0
static int ParticleEmitter_getObjectSpace(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<ParticleEmitter>(L, 1)->m_objectSpace);
    return 1;
}
// 0x0813afa0
static int ParticleEmitter_getClampToGroundPlane(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<ParticleEmitter>(L, 1)->m_clampToGroundPlane);
    return 1;
}
// 0x0813a6f0
static int ParticleEmitter_getDepthBias(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<ParticleEmitter>(L, 1)->m_depthBias);
    return 1;
}

const luaL_Reg ParticleEmitter_methods[] = {
    {"create", ParticleEmitter_create},
    {"setEmissionRate", ParticleEmitter_setEmissionRate},
    {"setEmissionTime", ParticleEmitter_setEmissionTime},
    {"setMaxParticles", ParticleEmitter_setMaxParticles},
    {"setSpawnBurst", ParticleEmitter_setSpawnBurst},
    {"setSprayAngle", ParticleEmitter_setSprayAngle},
    {"setBoxMin", ParticleEmitter_setBoxMin},
    {"setBoxMax", ParticleEmitter_setBoxMax},
    {"setMesh", ParticleEmitter_setMesh},
    {"setSkeleton", ParticleEmitter_setSkeleton},
    {"setVelocity", ParticleEmitter_setVelocity},
    {"setTexture", ParticleEmitter_setTexture},
    {"setFrameRate", ParticleEmitter_setFrameRate},
    {"setFrameSize", ParticleEmitter_setFrameSize},
    {"setFrameCount", ParticleEmitter_setFrameCount},
    {"setLifetime", ParticleEmitter_setLifetime},
    {"setColor", ParticleEmitter_setColor},
    {"setColorAnimation", ParticleEmitter_setColorAnimation},
    {"setOpacity", ParticleEmitter_setOpacity},
    {"setFadeIn", ParticleEmitter_setFadeIn},
    {"setFadeOut", ParticleEmitter_setFadeOut},
    {"setSize", ParticleEmitter_setSize},
    {"setGravity", ParticleEmitter_setGravity},
    {"setAirResistance", ParticleEmitter_setAirResistance},
    {"setRotationSpeed", ParticleEmitter_setRotationSpeed},
    {"setBlendMode", ParticleEmitter_setBlendMode},
    {"setObjectSpace", ParticleEmitter_setObjectSpace},
    {"setClampToGroundPlane", ParticleEmitter_setClampToGroundPlane},
    {"setDepthBias", ParticleEmitter_setDepthBias},
    {"getEmissionRate", ParticleEmitter_getEmissionRate},
    {"getEmissionTime", ParticleEmitter_getEmissionTime},
    {"getMaxParticles", ParticleEmitter_getMaxParticles},
    {"getSpawnBurst", ParticleEmitter_getSpawnBurst},
    {"getSprayAngle", ParticleEmitter_getSprayAngle},
    {"getBoxMin", ParticleEmitter_getBoxMin},
    {"getBoxMax", ParticleEmitter_getBoxMax},
    {"getMesh", ParticleEmitter_getMesh},
    {"getSkeleton", ParticleEmitter_getSkeleton},
    {"getVelocity", ParticleEmitter_getVelocity},
    {"getTexture", ParticleEmitter_getTexture},
    {"getFrameRate", ParticleEmitter_getFrameRate},
    {"getFrameSize", ParticleEmitter_getFrameSize},
    {"getFrameCount", ParticleEmitter_getFrameCount},
    {"getLifetime", ParticleEmitter_getLifetime},
    {"getColor", ParticleEmitter_getColor},
    {"getColorAnimation", ParticleEmitter_getColorAnimation},
    {"getOpacity", ParticleEmitter_getOpacity},
    {"getFadeIn", ParticleEmitter_getFadeIn},
    {"getFadeOut", ParticleEmitter_getFadeOut},
    {"getSize", ParticleEmitter_getSize},
    {"getGravity", ParticleEmitter_getGravity},
    {"getAirResistance", ParticleEmitter_getAirResistance},
    {"getRotationSpeed", ParticleEmitter_getRotationSpeed},
    {"getBlendMode", ParticleEmitter_getBlendMode},
    {"getObjectSpace", ParticleEmitter_getObjectSpace},
    {"getClampToGroundPlane", ParticleEmitter_getClampToGroundPlane},
    {"getDepthBias", ParticleEmitter_getDepthBias},
    {0, 0}};
const char* ParticleEmitter_properties[] = {"EmissionRate",
                                            "EmissionTime",
                                            "MaxParticles",
                                            "SpawnBurst",
                                            "SprayAngle",
                                            "BoxMin",
                                            "BoxMax",
                                            "Mesh",
                                            "Skeleton",
                                            "Velocity",
                                            "Texture",
                                            "FrameRate",
                                            "FrameSize",
                                            "FrameCount",
                                            "Lifetime",
                                            "Color",
                                            "ColorAnimation",
                                            "Opacity",
                                            "FadeIn",
                                            "FadeOut",
                                            "Size",
                                            "Gravity",
                                            "AirResistance",
                                            "RotationSpeed",
                                            "BlendMode",
                                            "ObjectSpace",
                                            "ClampToGroundPlane",
                                            "DepthBias",
                                            0};

// ---- MeshCDF ---------------------------------------------------------------------

// 0x0814afa0
static int MeshCDF_create(lua_State* L)
{
    Mesh* mesh = luax::checkObject<Mesh>(L, 1);
    luax::createSharedObject<MeshCDF>(L, new MeshCDF(mesh));
    return 1;
}

const luaL_Reg MeshCDF_methods[] = {{"create", MeshCDF_create}, {0, 0}};
