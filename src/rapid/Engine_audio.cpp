// Audio bindings of Engine.cpp: AudioEngine, AudioWorld, AudioListener, Sample,
// SoundSource.
#include "EngineBindings.h"
#include "core/Exception.h"

using namespace core;
using namespace engine;

// ---- AudioEngine -----------------------------------------------------------------

// 0x08145d10
static int AudioEngine_active(lua_State* L)
{
    if (!g_pEngineSystems)
        luaL_error(L, "engine not initialized");
    pushSharedObject<AudioEngine>(L, AudioEngine::sm_pActiveAudioEngine);
    return 1;
}
// 0x0813d030
static int AudioEngine_beginFrame(lua_State* L)
{
    luax::checkObject<AudioEngine>(L, 1)->beginFrame();
    return 0;
}
// 0x0813d1e0: render(world, listener)
static int AudioEngine_render(lua_State* L)
{
    try
    {
        AudioEngine* engine = luax::checkObject<AudioEngine>(L, 1);
        AudioWorld* world = luax::checkObject<AudioWorld>(L, 2);
        AudioListener* listener = luax::checkObject<AudioListener>(L, 3);
        engine->render(*world, *listener);
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x0813cfd0
static int AudioEngine_endFrame(lua_State* L)
{
    luax::checkObject<AudioEngine>(L, 1)->endFrame();
    return 0;
}

const luaL_Reg AudioEngine_methods[] = {{"active", AudioEngine_active},
                                        {"beginFrame", AudioEngine_beginFrame},
                                        {"render", AudioEngine_render},
                                        {"endFrame", AudioEngine_endFrame},
                                        {0, 0}};

// ---- AudioWorld ------------------------------------------------------------------

// 0x08143ee0
static int AudioWorld_create(lua_State* L)
{
    try
    {
        AudioWorld* world = AudioEngine::sm_pActiveAudioEngine->createWorld();
        luax::createSharedObject<AudioWorld>(L, world);
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x0813d300
static int AudioWorld_setVolume(lua_State* L)
{
    AudioWorld* world = luax::checkObject<AudioWorld>(L, 1);
    world->setVolume((float)luaL_checknumber(L, 2));
    return 0;
}
// 0x0813d100
static int AudioWorld_getVolume(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<AudioWorld>(L, 1)->getVolume());
    return 1;
}

const luaL_Reg AudioWorld_methods[] = {{"create", AudioWorld_create},
                                       {"setVolume", AudioWorld_setVolume},
                                       {"getVolume", AudioWorld_getVolume},
                                       {0, 0}};

// ---- AudioListener ---------------------------------------------------------------

// 0x08143dd0
static int AudioListener_create(lua_State* L)
{
    try
    {
        AudioListener* listener = AudioEngine::sm_pActiveAudioEngine->createListener();
        luax::createSharedObject<AudioListener>(L, listener);
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x0813bb10
static int AudioListener_setVolume(lua_State* L)
{
    AudioListener* listener = luax::checkObject<AudioListener>(L, 1);
    listener->setVolume((float)luaL_checknumber(L, 2));
    return 0;
}
// 0x0813d090
static int AudioListener_getVolume(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<AudioListener>(L, 1)->getVolume());
    return 1;
}

const luaL_Reg AudioListener_methods[] = {{"create", AudioListener_create},
                                          {"setVolume", AudioListener_setVolume},
                                          {"getVolume", AudioListener_getVolume},
                                          {0, 0}};

// ---- Sample ----------------------------------------------------------------------

// 0x081468f0
static int Sample_load(lua_State* L)
{
    try
    {
        const char* filename = luaL_checkstring(L, 1);
        luax::createSharedObject<Sample>(L, loadSample(filename));
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x0813cdd0
static int Sample_reload(lua_State* L)
{
    try
    {
        luax::checkObject<Sample>(L, 1)->reload();
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x08143c80
static int Sample_getSampleByFilename(lua_State* L)
{
    const char* filename = luaL_checkstring(L, 1);
    pushSharedObject<Sample>(L, Sample::getSampleByFilename(filename));
    return 1;
}

const luaL_Reg Sample_methods[] = {{"load", Sample_load},
                                   {"reload", Sample_reload},
                                   {"getSampleByFilename", Sample_getSampleByFilename},
                                   {0, 0}};
const char* Sample_properties[] = {0};

// ---- SoundSource -----------------------------------------------------------------

// 0x081467e0: SoundSource.create(world)
static int SoundSource_create(lua_State* L)
{
    AudioWorld* world = luax::checkObject<AudioWorld>(L, 1);
    luax::createSharedObject<SoundSource>(L, world->createSoundSource());
    return 1;
}
// 0x0814d260: play(sample, positional)
static int SoundSource_play(lua_State* L)
{
    try
    {
        SoundSource* source = luax::checkObject<SoundSource>(L, 1);
        Sample* sample = luax::checkObject<Sample>(L, 2);
        bool positional = luax::checkBool(L, 3);
        source->play(*sample, positional);
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x08140f90
static int SoundSource_playStream(lua_State* L)
{
    try
    {
        SoundSource* source = luax::checkObject<SoundSource>(L, 1);
        const char* filename = luaL_checkstring(L, 2);
        source->playStream(filename);
        return 0;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x08140f30
static int SoundSource_stop(lua_State* L)
{
    luax::checkObject<SoundSource>(L, 1)->stop();
    return 0;
}
// 0x081411a0
static int SoundSource_isPlaying(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<SoundSource>(L, 1)->isPlaying());
    return 1;
}
// 0x08141310
static int SoundSource_setVolume(lua_State* L)
{
    SoundSource* source = luax::checkObject<SoundSource>(L, 1);
    source->setVolume((float)luaL_checknumber(L, 2));
    return 0;
}
// 0x0814d1c0
static int SoundSource_setMute(lua_State* L)
{
    SoundSource* source = luax::checkObject<SoundSource>(L, 1);
    source->setMute(luax::checkBool(L, 2));
    return 0;
}
// 0x08141290
static int SoundSource_setMinDistance(lua_State* L)
{
    SoundSource* source = luax::checkObject<SoundSource>(L, 1);
    source->setMinDistance((float)luaL_checknumber(L, 2));
    return 0;
}
// 0x08141210
static int SoundSource_setMaxDistance(lua_State* L)
{
    SoundSource* source = luax::checkObject<SoundSource>(L, 1);
    source->setMaxDistance((float)luaL_checknumber(L, 2));
    return 0;
}
// 0x0814d120
static int SoundSource_setLoop(lua_State* L)
{
    SoundSource* source = luax::checkObject<SoundSource>(L, 1);
    source->setLoop(luax::checkBool(L, 2));
    return 0;
}
// 0x08140ec0
static int SoundSource_getVolume(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<SoundSource>(L, 1)->getVolume());
    return 1;
}
// 0x08141130
static int SoundSource_getMute(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<SoundSource>(L, 1)->getMute());
    return 1;
}
// 0x08140e50
static int SoundSource_getMinDistance(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<SoundSource>(L, 1)->getMinDistance());
    return 1;
}
// 0x08141050
static int SoundSource_getMaxDistance(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<SoundSource>(L, 1)->getMaxDistance());
    return 1;
}
// 0x081410c0
static int SoundSource_getLoop(lua_State* L)
{
    lua_pushboolean(L, luax::checkObject<SoundSource>(L, 1)->getLoop());
    return 1;
}
// 0x08140de0
static int SoundSource_getSamplePosition(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<SoundSource>(L, 1)->getSamplePosition());
    return 1;
}
// 0x08141390
static int SoundSource_getNode(lua_State* L)
{
    SoundSource* source = luax::checkObject<SoundSource>(L, 1);
    luax::pushObject(L, source->getNode());
    return 1;
}

const luaL_Reg SoundSource_methods[] = {{"create", SoundSource_create},
                                        {"play", SoundSource_play},
                                        {"playStream", SoundSource_playStream},
                                        {"stop", SoundSource_stop},
                                        {"isPlaying", SoundSource_isPlaying},
                                        {"setVolume", SoundSource_setVolume},
                                        {"setMute", SoundSource_setMute},
                                        {"setMinDistance", SoundSource_setMinDistance},
                                        {"setMaxDistance", SoundSource_setMaxDistance},
                                        {"setLoop", SoundSource_setLoop},
                                        {"getVolume", SoundSource_getVolume},
                                        {"getMute", SoundSource_getMute},
                                        {"getMinDistance", SoundSource_getMinDistance},
                                        {"getMaxDistance", SoundSource_getMaxDistance},
                                        {"getLoop", SoundSource_getLoop},
                                        {"getSamplePosition", SoundSource_getSamplePosition},
                                        {"getNode", SoundSource_getNode},
                                        {0, 0}};
const char* SoundSource_properties[] = {"Volume", "MinDistance", "MaxDistance", "Loop", 0};
