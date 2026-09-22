// HeightmapBuilder bindings of Legend of Grimrock 2 (0x0042fb80-0x0042fd10).
#include "EngineBindings.h"
#include "engine/HeightmapBuilder.h"
#include "sys.h" // core LUAX_CLASS declarations (Image ...)

using namespace core;
using namespace engine;

LUAX_CLASS(engine::HeightmapBuilder, "HeightmapBuilder")

// 0x0042fb80
static int HeightmapBuilder_create(lua_State* L)
{
    luax::createSharedObject<HeightmapBuilder>(L, new HeightmapBuilder);
    return 1;
}
// 0x0042fbe0: tessellateTile(x, y, elevation, subdivisions, heightmap | nil, blendMap | nil,
// w00, w10, w01, w11)
static int HeightmapBuilder_tessellateTile(lua_State* L)
{
    HeightmapBuilder* builder = luax::checkObject<HeightmapBuilder>(L, 1);
    int x = luaL_checkinteger(L, 2);
    int y = luaL_checkinteger(L, 3);
    int elevation = luaL_checkinteger(L, 4);
    int subdivisions = luaL_checkinteger(L, 5);
    if (subdivisions < 1)
        luaL_argerror(L, 5, "invalid subdivision count");
    Image* heightmap = luax::checkObjectOpt<Image>(L, 6);
    Image* blendMap = luax::checkObjectOpt<Image>(L, 7);
    float w00 = (float)luaL_checknumber(L, 8);
    float w10 = (float)luaL_checknumber(L, 9);
    float w01 = (float)luaL_checknumber(L, 10);
    float w11 = (float)luaL_checknumber(L, 11);
    builder->tessellateTile(x, y, elevation, subdivisions, heightmap, blendMap, w00, w10, w01, w11);
    return 0;
}
// 0x0042fcc0
static int HeightmapBuilder_createMesh(lua_State* L)
{
    HeightmapBuilder* builder = luax::checkObject<HeightmapBuilder>(L, 1);
    luax::createSharedObject<Mesh>(L, builder->createMesh());
    return 1;
}
// 0x0042fd10
static int HeightmapBuilder_getNumIndices(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<HeightmapBuilder>(L, 1)->getNumIndices());
    return 1;
}

const luaL_Reg HeightmapBuilder_methods[] = {{"create", HeightmapBuilder_create},
                                             {"tessellateTile", HeightmapBuilder_tessellateTile},
                                             {"createMesh", HeightmapBuilder_createMesh},
                                             {"getNumIndices", HeightmapBuilder_getNumIndices},
                                             {0, 0}};
