// Animation, AnimationController, AnimationState and AssetProcessor bindings of
// Engine.cpp.
#include "EngineBindings.h"
#include "core/Exception.h"

using namespace core;
using namespace engine;

// ---- Animation -------------------------------------------------------------------

// 0x081466c0
static int Animation_load(lua_State* L)
{
    try
    {
        const char* filename = luaL_checkstring(L, 1);
        luax::createSharedObject<Animation>(L, loadAnimation(filename));
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x0813f930
static int Animation_setName(lua_State* L)
{
    Animation* animation = luax::checkObject<Animation>(L, 1);
    animation->setName(luaL_checkstring(L, 2));
    return 0;
}
// 0x0813f600
static int Animation_setFramesPerSecond(lua_State* L)
{
    Animation* animation = luax::checkObject<Animation>(L, 1);
    animation->setFrameRate((float)luaL_checknumber(L, 2));
    return 0;
}
// 0x0813f590
static int Animation_setFrameCount(lua_State* L)
{
    Animation* animation = luax::checkObject<Animation>(L, 1);
    animation->setFrameCount(luaL_checkinteger(L, 2));
    return 0;
}
// 0x0813f520
static int Animation_getName(lua_State* L)
{
    lua_pushstring(L, luax::checkObject<Animation>(L, 1)->getName().c_str());
    return 1;
}
// 0x0813f080
static int Animation_getFramesPerSecond(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<Animation>(L, 1)->getFrameRate());
    return 1;
}
// 0x0813f010
static int Animation_getFrameCount(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<Animation>(L, 1)->getFrameCount());
    return 1;
}
// 0x0813efa0
static int Animation_getLength(lua_State* L)
{
    Animation* animation = luax::checkObject<Animation>(L, 1);
    lua_pushnumber(L, (float)animation->getFrameCount() / animation->getFrameRate());
    return 1;
}
// 0x0813f670: addEvent(time, name)
static int Animation_addEvent(lua_State* L)
{
    Animation* animation = luax::checkObject<Animation>(L, 1);
    float time = (float)luaL_checknumber(L, 2);
    const char* name = luaL_checkstring(L, 3);
    animation->addEvent(time, name);
    return 0;
}

#if GRIMROCK_GAME >= 2
// 0x0041ea10
static int Animation_getFilename(lua_State* L)
{
    lua_pushstring(L, luax::checkObject<Animation>(L, 1)->getFilename().c_str());
    return 1;
}
// 0x0041eac0
static int Animation_removeEvents(lua_State* L)
{
    luax::checkObject<Animation>(L, 1)->removeEvents();
    return 0;
}
#endif

const luaL_Reg Animation_methods[] = {{"load", Animation_load},
                                      {"setName", Animation_setName},
                                      {"setFramesPerSecond", Animation_setFramesPerSecond},
                                      {"setFrameCount", Animation_setFrameCount},
                                      {"getName", Animation_getName},
                                      {"getFramesPerSecond", Animation_getFramesPerSecond},
                                      {"getFrameCount", Animation_getFrameCount},
                                      {"getLength", Animation_getLength},
#if GRIMROCK_GAME >= 2
                                      {"getFilename", Animation_getFilename},
#endif
                                      {"addEvent", Animation_addEvent},
#if GRIMROCK_GAME >= 2
                                      {"removeEvents", Animation_removeEvents},
#endif
                                      {0, 0}};
const char* Animation_properties[] = {"Name", "FramesPerSecond", "FrameCount", "Length", 0};

// ---- AnimationController ---------------------------------------------------------

// 0x081498f0: AnimationController.create(rootNode)
static int AnimationController_create(lua_State* L)
{
#if GRIMROCK_GAME >= 2
    luax::createSharedObject<AnimationController>(L, new AnimationController());
#else
    Node* root = luax::checkObject<Node>(L, 1);
    luax::createSharedObject<AnimationController>(L, new AnimationController(root));
#endif
    return 1;
}
#if GRIMROCK_GAME >= 2
// 0x0041eb80: bind(rootNode)
static int AnimationController_bind(lua_State* L)
{
    AnimationController* controller = luax::checkObject<AnimationController>(L, 1);
    Node* root = luax::checkObject<Node>(L, 2);
    controller->bind(root);
    return 0;
}
// 0x0041ecf0: crossfade(name, fadeTime [, loop [, layer]])
static int AnimationController_crossfade(lua_State* L)
{
    AnimationController* controller = luax::checkObject<AnimationController>(L, 1);
    const char* name = luaL_checkstring(L, 2);
    float fadeTime = (float)luaL_checknumber(L, 3);
    bool loop = false;
    if (lua_type(L, 4) != LUA_TNONE)
        loop = luax::checkBool(L, 4);
    int layer = luaL_optinteger(L, 5, 0);
    if (!controller->crossfade(name, fadeTime, loop, layer))
        luaL_error(L, "invalid animation state %s", name);
    return 0;
}
#endif
// 0x0813f460: addClip(animation, name)
static int AnimationController_addClip(lua_State* L)
{
    AnimationController* controller = luax::checkObject<AnimationController>(L, 1);
    Animation* animation = luax::checkObject<Animation>(L, 2);
    const char* name = luaL_checkstring(L, 3);
    controller->addClip(*animation, name);
    return 0;
}
// 0x08151ba0: play(name [, loop [, layer]])
static int AnimationController_play(lua_State* L)
{
    AnimationController* controller = luax::checkObject<AnimationController>(L, 1);
    bool loop = false;
    const char* name = luaL_checkstring(L, 2);
    if (lua_type(L, 3) != LUA_TNONE)
        loop = luax::checkBool(L, 3);
    int layer = luaL_optinteger(L, 4, 0);
    if (!controller->play(name, loop, layer))
        luaL_error(L, "invalid animation state %s", name);
    return 0;
}
// 0x0813f400
static int AnimationController_stop(lua_State* L)
{
    luax::checkObject<AnimationController>(L, 1)->stop();
    return 0;
}
// 0x0813f7d0
static int AnimationController_isPlaying(lua_State* L)
{
    AnimationController* controller = luax::checkObject<AnimationController>(L, 1);
#if GRIMROCK_GAME >= 2
    if (lua_gettop(L) < 2)
    {
        lua_pushboolean(L, controller->isPlaying());
        return 1;
    }
#endif
    const char* name = luaL_checkstring(L, 2);
    lua_pushboolean(L, controller->isPlaying(name));
    return 1;
}
// 0x08145be0: getAnimationState(name | index)
static int AnimationController_getAnimationState(lua_State* L)
{
    AnimationController* controller = luax::checkObject<AnimationController>(L, 1);
    if (!lua_isnumber(L, 2))
    {
        const char* name = luaL_checkstring(L, 2);
        AnimationState* state = controller->getAnimationState(name);
#if GRIMROCK_GAME >= 2
        // 0x0041ee60: nil for an unknown name
        if (!state)
        {
            lua_pushnil(L);
            return 1;
        }
#else
        if (!state)
            luaL_error(L, "no animation state with name: %s", name);
#endif
        pushSharedObject<AnimationState>(L, state);
    }
    else
    {
        int index = luaL_checkinteger(L, 2) - 1;
        if (index < 0 || index >= controller->getAnimationStateCount())
            luaL_error(L, "invalid animation state index");
        pushSharedObject<AnimationState>(L, controller->getAnimationState(index));
    }
    return 1;
}
// 0x0813f760
static int AnimationController_getAnimationStateCount(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<AnimationController>(L, 1)->getAnimationStateCount());
    return 1;
}
// 0x0813f700
static int AnimationController_sample(lua_State* L)
{
    luax::checkObject<AnimationController>(L, 1)->sample();
    return 0;
}
// 0x0813f8c0
static int AnimationController_advance(lua_State* L)
{
    AnimationController* controller = luax::checkObject<AnimationController>(L, 1);
    controller->advance((float)luaL_checknumber(L, 2));
    return 0;
}
// 0x0813f850
static int AnimationController_update(lua_State* L)
{
    AnimationController* controller = luax::checkObject<AnimationController>(L, 1);
    controller->update((float)luaL_checknumber(L, 2));
    return 0;
}
// 0x0813bb90: names of the events triggered by the last advance, nil when none
static int AnimationController_getEvents(lua_State* L)
{
    AnimationController* controller = luax::checkObject<AnimationController>(L, 1);
    const Array<const Animation::Event*>& events = controller->getTriggeredEvents();
    if (events.size() != 0)
    {
        lua_createtable(L, 0, 0);
        for (int i = 0; i < events.size(); ++i)
        {
            lua_pushstring(L, events[i]->name.c_str());
            lua_rawseti(L, -2, i + 1);
        }
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

const luaL_Reg AnimationController_methods[] = {
    {"create", AnimationController_create},
#if GRIMROCK_GAME >= 2
    {"bind", AnimationController_bind},
#endif
    {"addClip", AnimationController_addClip},
    {"play", AnimationController_play},
#if GRIMROCK_GAME >= 2
    {"crossfade", AnimationController_crossfade},
#endif
    {"stop", AnimationController_stop},
    {"isPlaying", AnimationController_isPlaying},
    {"getAnimationState", AnimationController_getAnimationState},
    {"getAnimationStateCount", AnimationController_getAnimationStateCount},
    {"sample", AnimationController_sample},
    {"advance", AnimationController_advance},
    {"update", AnimationController_update},
    {"getEvents", AnimationController_getEvents},
    {0, 0}};

// ---- AnimationState --------------------------------------------------------------

// 0x08144e40
static int AnimationState_setAnimation(lua_State* L)
{
    AnimationState* state = luax::checkObject<AnimationState>(L, 1);
    Animation* animation = luax::checkObject<Animation>(L, 2);
    state->m_animation.reset(animation);
    return 0;
}
// 0x081405c0
static int AnimationState_setLayer(lua_State* L)
{
    luax::checkObject<AnimationState>(L, 1)->m_layer = luaL_checkinteger(L, 2);
    return 0;
}
// 0x0813baa0
static int AnimationState_setTime(lua_State* L)
{
    luax::checkObject<AnimationState>(L, 1)->m_time = (float)luaL_checknumber(L, 2);
    return 0;
}
// 0x081406a0
static int AnimationState_setWeight(lua_State* L)
{
    luax::checkObject<AnimationState>(L, 1)->m_weight = (float)luaL_checknumber(L, 2);
    return 0;
}
// 0x08140630
static int AnimationState_setSpeed(lua_State* L)
{
    luax::checkObject<AnimationState>(L, 1)->m_speed = (float)luaL_checknumber(L, 2);
    return 0;
}
// 0x0814d0a0
static int AnimationState_setLoop(lua_State* L)
{
    AnimationState* state = luax::checkObject<AnimationState>(L, 1);
    state->m_loop = luax::checkBool(L, 2);
    return 0;
}
// 0x0814e050
static int AnimationState_setEnabled(lua_State* L)
{
    AnimationState* state = luax::checkObject<AnimationState>(L, 1);
    state->m_playing = luax::checkBool(L, 2);
    return 0;
}
// 0x081439a0
static int AnimationState_getAnimation(lua_State* L)
{
    pushSharedObject<Animation>(L, luax::checkObject<AnimationState>(L, 1)->getAnimation());
    return 1;
}
// 0x08140470
static int AnimationState_getName(lua_State* L)
{
    lua_pushstring(L, luax::checkObject<AnimationState>(L, 1)->getName().c_str());
    return 1;
}
// 0x08140400
static int AnimationState_getLayer(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<AnimationState>(L, 1)->m_layer);
    return 1;
}
// 0x08140390
static int AnimationState_getTime(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<AnimationState>(L, 1)->m_time);
    return 1;
}
// 0x08140320
static int AnimationState_getWeight(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<AnimationState>(L, 1)->m_weight);
    return 1;
}
// 0x08140240
static int AnimationState_getSpeed(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<AnimationState>(L, 1)->m_speed);
    return 1;
}
// 0x08140550
static int AnimationState_getLoop(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<AnimationState>(L, 1)->m_loop);
    return 1;
}
// 0x081404e0
static int AnimationState_getEnabled(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<AnimationState>(L, 1)->m_playing);
    return 1;
}
// 0x081402b0
static int AnimationState_advance(lua_State* L)
{
    AnimationState* state = luax::checkObject<AnimationState>(L, 1);
    state->advance((float)luaL_checknumber(L, 2));
    return 1;
}

const luaL_Reg AnimationState_methods[] = {{"setAnimation", AnimationState_setAnimation},
                                           {"setLayer", AnimationState_setLayer},
                                           {"setTime", AnimationState_setTime},
                                           {"setWeight", AnimationState_setWeight},
                                           {"setSpeed", AnimationState_setSpeed},
                                           {"setLoop", AnimationState_setLoop},
                                           {"setEnabled", AnimationState_setEnabled},
                                           {"getAnimation", AnimationState_getAnimation},
                                           {"getName", AnimationState_getName},
                                           {"getLayer", AnimationState_getLayer},
                                           {"getTime", AnimationState_getTime},
                                           {"getWeight", AnimationState_getWeight},
                                           {"getSpeed", AnimationState_getSpeed},
                                           {"getLoop", AnimationState_getLoop},
                                           {"getEnabled", AnimationState_getEnabled},
                                           {"advance", AnimationState_advance},
                                           {0, 0}};
const char* AnimationState_properties[] = {"Animation", "Name", "Layer",   "Time", "Weight",
                                           "Speed",     "Loop", "Enabled", 0};

// ---- AssetProcessor --------------------------------------------------------------

// 0x08145940: AssetProcessor.find(type, filename)
static int AssetProcessor_find(lua_State* L)
{
    try
    {
        luax::Enum types[] = {{"Model", 0}, {"Animation", 1}, {"Texture", 2}, {0, 0}};
        int type = luax::checkEnum(L, 1, types);
        const char* filename = luaL_checkstring(L, 2);
        AssetProcessor* processor = findAssetProcessor((AssetProcessor::AssetType)type, filename);
        pushSharedObject<AssetProcessor>(L, processor);
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x0813d550
static int AssetProcessor_processFile(lua_State* L)
{
    try
    {
        AssetProcessor* processor = luax::checkObject<AssetProcessor>(L, 1);
        const char* filename = luaL_checkstring(L, 2);
        processor->processFile(filename);
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x0813d4e0
static int AssetProcessor_getFileExtension(lua_State* L)
{
    lua_pushstring(L, luax::checkObject<AssetProcessor>(L, 1)->getFileExtension().c_str());
    return 1;
}
// 0x0813d470
static int AssetProcessor_getNativeFileExtension(lua_State* L)
{
    lua_pushstring(L, luax::checkObject<AssetProcessor>(L, 1)->getNativeFileExtension().c_str());
    return 1;
}
// 0x0813c9d0
static int AssetProcessor_getNativeFile(lua_State* L)
{
    AssetProcessor* processor = luax::checkObject<AssetProcessor>(L, 1);
    const char* filename = luaL_checkstring(L, 2);
    String native = processor->getNativeFile(filename);
    lua_pushstring(L, native.c_str());
    return 1;
}

const luaL_Reg AssetProcessor_methods[] = {
    {"find", AssetProcessor_find},
    {"processFile", AssetProcessor_processFile},
    {"getFileExtension", AssetProcessor_getFileExtension},
    {"getNativeFileExtension", AssetProcessor_getNativeFileExtension},
    {"getNativeFile", AssetProcessor_getNativeFile},
    {0, 0}};
