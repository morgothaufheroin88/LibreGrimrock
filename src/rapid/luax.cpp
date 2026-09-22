// Reconstructed from Grimrock.bin.x86 luax.cpp.
#include "luax.h"
#include "core/Exception.h"
#include "core/FileSystem.h"
#include "core/String.h"
#include "core/Sys.h"
#include <cstdio>
#include <cstring>

using namespace core;

namespace luax
{

// ---- root class: tostring, dispose, isDisposed ---------------------------------------

// 0x0812a8b0
static int tostring(lua_State* L)
{
    Proxy* proxy = (Proxy*)lua_touserdata(L, 1);
    lua_getmetatable(L, 1);
    lua_pushstring(L, "type");
    lua_rawget(L, -2);
    char buf[64];
    snprintf(buf, sizeof(buf), "%s: %proxy", lua_tostring(L, -1), proxy->object);
    lua_pushstring(L, buf);
    return 1;
}

// Walks the baseclass chain and checks it ends in rapid.root_class (0x0812a950).
static Proxy* checkRootObject(lua_State* L, int index)
{
    Proxy* proxy = (Proxy*)lua_touserdata(L, index);
    if (!proxy)
        luaL_error(L, "not a valid object");
    lua_getmetatable(L, index);
    if (lua_type(L, -1) != LUA_TTABLE)
        luaL_error(L, "not a valid object");
    for (;;)
    {
        lua_pushstring(L, "baseclass");
        lua_rawget(L, -2);
        if (lua_type(L, -1) != LUA_TTABLE)
            break;
        lua_remove(L, -2);
    }
    lua_pop(L, 1);
    lua_getfield(L, LUA_REGISTRYINDEX, "rapid.root_class");
    if (!lua_rawequal(L, -1, -2))
        luaL_error(L, "not a valid object");
    lua_pop(L, 2);
    return proxy;
}

// 0x0812a950
static int isDisposed(lua_State* L)
{
    Proxy* proxy = checkRootObject(L, 1);
    lua_pushboolean(L, proxy->object == 0);
    return 1;
}

// 0x0812bb10: also the __gc of every proxy.
static int dispose(lua_State* L)
{
    Proxy* proxy = checkRootObject(L, 1);
    if (proxy->object)
    {
        lua_getfield(L, LUA_REGISTRYINDEX, "rapid.object_map");
        lua_pushlightuserdata(L, proxy->object);
        lua_pushnil(L);
        lua_rawset(L, -3);
        lua_pop(L, 1);
        if (proxy->destroy)
            proxy->destroy(proxy->object);
        proxy->object = 0;
    }
    return 0;
}

// ---- uint64 userdata -------------------------------------------------------------

static int uint64_tostring(lua_State* L)
{
    unsigned long long* v = (unsigned long long*)luaL_checkudata(L, 1, "rapid.uint64");
    char buf[64];
    snprintf(buf, sizeof(buf), "uint64: 0x%08llx", *v);
    lua_pushstring(L, buf);
    return 1;
}
static int uint64_equals(lua_State* L)
{
    unsigned long long* a = (unsigned long long*)luaL_checkudata(L, 1, "rapid.uint64");
    unsigned long long* b = (unsigned long long*)luaL_checkudata(L, 2, "rapid.uint64");
    lua_pushboolean(L, *a == *b);
    return 1;
}
static int uint64_lessThan(lua_State* L)
{
    unsigned long long* a = (unsigned long long*)luaL_checkudata(L, 1, "rapid.uint64");
    unsigned long long* b = (unsigned long long*)luaL_checkudata(L, 2, "rapid.uint64");
    lua_pushboolean(L, *a < *b);
    return 1;
}
static int uint64_lessEqual(lua_State* L)
{
    unsigned long long* a = (unsigned long long*)luaL_checkudata(L, 1, "rapid.uint64");
    unsigned long long* b = (unsigned long long*)luaL_checkudata(L, 2, "rapid.uint64");
    lua_pushboolean(L, *a <= *b);
    return 1;
}
// 0x081343b0
void pushUInt64(lua_State* L, unsigned long long v)
{
    unsigned long long* p = (unsigned long long*)lua_newuserdata(L, sizeof(unsigned long long));
    luaL_getmetatable(L, "rapid.uint64");
    lua_setmetatable(L, -2);
    *p = v;
}
unsigned long long checkUInt64(lua_State* L, int index)
{
    return *(unsigned long long*)luaL_checkudata(L, index, "rapid.uint64");
}

// ---- init ------------------------------------------------------------------------

// 0x0812aa90
void init(lua_State* L)
{
    createWeakTable(L, "kv");
    lua_setfield(L, LUA_REGISTRYINDEX, "rapid.object_map");
    luaL_newmetatable(L, "rapid.root_class");
    lua_pushcfunction(L, tostring);
    lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, dispose);
    lua_setfield(L, -2, "__gc");
    lua_pushcfunction(L, dispose);
    lua_setfield(L, -2, "dispose");
    lua_pushcfunction(L, isDisposed);
    lua_setfield(L, -2, "isDisposed");
    lua_pushstring(L, "rapid.root_class");
    lua_setfield(L, -2, "type");
    lua_pop(L, 1);
    luaL_newmetatable(L, "rapid.uint64");
    lua_pushcfunction(L, uint64_tostring);
    lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, uint64_equals);
    lua_setfield(L, -2, "__eq");
    lua_pushcfunction(L, uint64_lessThan);
    lua_setfield(L, -2, "__lt");
    lua_pushcfunction(L, uint64_lessEqual);
    lua_setfield(L, -2, "__le");
    lua_pop(L, 1);
}

// 0x0812a4a0
void createWeakTable(lua_State* L, const char* mode)
{
    lua_newtable(L);
    lua_newtable(L);
    lua_pushstring(L, mode);
    lua_setfield(L, -2, "__mode");
    lua_setmetatable(L, -2);
}

// 0x0812a520
void dumpStack(lua_State* L)
{
    for (int i = 1; i <= lua_gettop(L); ++i)
        debugPrint("%d - %s\n", i, lua_typename(L, lua_type(L, i)));
}

// 0x0812a460 / 0x0812a580
void setGlobal(lua_State* L, const char* name, const char* value)
{
    lua_pushstring(L, value);
    lua_setfield(L, LUA_GLOBALSINDEX, name);
}
void setGlobal(lua_State* L, const char* name, int value)
{
    lua_pushnumber(L, value);
    lua_setfield(L, LUA_GLOBALSINDEX, name);
}

// 0x0812a5c0
void registerEnums(lua_State* L, const char* table, Enum* enums)
{
    lua_getfield(L, LUA_GLOBALSINDEX, table);
    for (; enums->name; ++enums)
    {
        lua_pushnumber(L, enums->value);
        lua_setfield(L, -2, enums->name);
    }
    lua_pop(L, 1);
}

// 0x0812a640
void registerModule(lua_State* L, const char* name, const luaL_Reg* functions)
{
    static const luaL_Reg empty[] = {{0, 0}};
    if (!functions)
        functions = empty;
    luaL_register(L, name, functions);
    lua_pop(L, 1);
}

// 0x0812a680
void registerFunctions(lua_State* L, const luaL_Reg* functions)
{
    for (; functions->name; ++functions)
    {
        lua_pushcfunction(L, functions->func);
        lua_setfield(L, LUA_GLOBALSINDEX, functions->name);
    }
}

// 0x0812a3e0
int checkEnum(lua_State* L, int index, const Enum* enums)
{
    const char* name = luaL_checkstring(L, index);
    for (; enums->name; ++enums)
        if (strcmp(enums->name, name) == 0)
            return enums->value;
    luaL_argerror(L, index, "invalid enum value");
    return -1;
}

// 0x08134360
void pushEnum(lua_State* L, int value, const Enum* enums)
{
    for (; enums->name; ++enums)
    {
        if (enums->value == value)
        {
            lua_pushstring(L, enums->name);
            return;
        }
    }
    lua_pushstring(L, "???");
}

// ---- properties ------------------------------------------------------------------

// 0x0812b670: obj.Prop = v  ->  obj:setProp(v)
static int setProperty(lua_State* L)
{
    if (!luaL_getmetafield(L, 1, "properties"))
        luaL_argerror(L, 2, "object has no properties");
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);
    if (lua_isnil(L, -1))
    {
        luaL_where(L, 1);
        lua_pushfstring(L, "attempt to set invalid property '%s'", lua_tostring(L, 2));
        lua_concat(L, 2);
        lua_error(L);
    }
    lua_pop(L, 2);
    String setter = formatString("set%s", lua_tostring(L, 2));
    if (!luaL_getmetafield(L, 1, setter.c_str()))
        luaL_argerror(L, 2, "property setter not found");
    lua_CFunction f = lua_tocfunction(L, -1);
    lua_remove(L, 2);
    lua_settop(L, 2);
    f(L);
    return 0;
}

// 0x0812b840: methods first, then obj.Prop -> obj:getProp()
static int getProperty(lua_State* L)
{
    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);
    if (lua_type(L, -1) == LUA_TFUNCTION)
        return 1;
    if (!luaL_getmetafield(L, 1, "properties"))
        luaL_argerror(L, 2, "object has no properties");
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);
    if (lua_isnil(L, -1))
        return 1;
    lua_pop(L, 2);
    String getter = formatString("get%s", lua_tostring(L, 2));
    if (!luaL_getmetafield(L, 1, getter.c_str()))
        luaL_argerror(L, 2, "internal error: property accessor not found");
    lua_CFunction f = lua_tocfunction(L, -1);
    lua_settop(L, 1);
    return f(L);
}

// 0x0812ad10 / 0x0812b1c0: shared by registerClass (base = rapid.root_class).
static void registerClassImpl(lua_State* L, const char* name, int baseIndex,
                              const luaL_Reg* methods, const char* const* properties)
{
    luaL_newmetatable(L, name);
    int classIndex = lua_gettop(L);
    // "Module.Class" lives in the module table, otherwise as a global
    const char* dot = strchr(name, '.');
    if (!dot)
    {
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_GLOBALSINDEX, name);
    }
    else
    {
        String module(name);
        module.erase(module.find(".", 0), module.size() - 1);
        lua_getfield(L, LUA_GLOBALSINDEX, module.c_str());
        lua_pushvalue(L, -2);
        lua_setfield(L, -2, dot + 1);
        lua_pop(L, 1);
    }
    lua_pushstring(L, name);
    lua_setfield(L, classIndex, "type");
    lua_pushvalue(L, baseIndex);
    lua_setfield(L, classIndex, "baseclass");
    lua_pushvalue(L, classIndex);
    lua_setfield(L, classIndex, "__index");
    // inherit the functions of the base class
    lua_pushnil(L);
    while (lua_next(L, baseIndex))
    {
        if (lua_isstring(L, -2) && lua_type(L, -1) == LUA_TFUNCTION)
        {
            lua_pushvalue(L, -2);
            lua_pushvalue(L, -2);
            lua_rawset(L, classIndex);
        }
        lua_pop(L, 1);
    }
    if (methods)
        luaL_register(L, 0, methods);
    // properties: inherited set plus the new names
    lua_newtable(L);
    int propsIndex = lua_gettop(L);
    bool hasProperties = false;
    lua_pushstring(L, "properties");
    lua_rawget(L, baseIndex);
    if (!lua_isnil(L, -1))
    {
        int baseProps = lua_gettop(L);
        lua_pushnil(L);
        while (lua_next(L, baseProps))
        {
            lua_pushvalue(L, -2);
            lua_pushboolean(L, 1);
            lua_rawset(L, propsIndex);
            lua_pop(L, 1);
            hasProperties = true;
        }
    }
    lua_pop(L, 1);
    if (properties)
    {
        for (; *properties; ++properties)
        {
            lua_pushstring(L, *properties);
            lua_pushboolean(L, 1);
            lua_rawset(L, propsIndex);
            hasProperties = true;
        }
    }
    lua_setfield(L, classIndex, "properties");
    if (hasProperties)
    {
        lua_pushcfunction(L, setProperty);
        lua_setfield(L, -2, "__newindex");
        lua_pushcfunction(L, getProperty);
        lua_setfield(L, -2, "__index");
    }
    lua_pop(L, 2);
}

void registerSubclass(lua_State* L, const char* name, const char* base, const luaL_Reg* methods,
                      const char* const* properties)
{
    lua_getfield(L, LUA_REGISTRYINDEX, base);
    registerClassImpl(L, name, lua_gettop(L), methods, properties);
}

void registerClass(lua_State* L, const char* name, const luaL_Reg* methods,
                   const char* const* properties)
{
    lua_getfield(L, LUA_REGISTRYINDEX, "rapid.root_class");
    registerClassImpl(L, name, lua_gettop(L), methods, properties);
}

// ---- objects ---------------------------------------------------------------------

// 0x08156080 (createObject<T>)
void createObjectRaw(lua_State* L, const char* className, void* object, void (*destroy)(void*))
{
    Proxy* proxy = (Proxy*)lua_newuserdata(L, sizeof(Proxy));
    proxy->object = object;
    proxy->destroy = destroy;
    lua_getfield(L, LUA_REGISTRYINDEX, className);
    lua_setmetatable(L, -2);
    lua_getfield(L, LUA_REGISTRYINDEX, "rapid.object_map");
    lua_pushlightuserdata(L, object);
    lua_pushvalue(L, -3);
    lua_rawset(L, -3);
    lua_pop(L, 1);
}

// grimrock2.exe 0x0042f380
int proxyCount(lua_State* L)
{
    lua_getfield(L, LUA_REGISTRYINDEX, "rapid.object_map");
    int table = lua_gettop(L);
    int count = 0;
    lua_pushnil(L);
    while (lua_next(L, table) != 0)
    {
        ++count;
        lua_settop(L, -2);
    }
    lua_settop(L, table - 1);
    return count;
}

// 0x08155f40
void pushObject(lua_State* L, void* object)
{
    if (!object)
    {
        lua_pushnil(L);
        return;
    }
    lua_getfield(L, LUA_REGISTRYINDEX, "rapid.object_map");
    lua_pushlightuserdata(L, object);
    lua_rawget(L, -2);
    lua_remove(L, -2);
}

// 0x081555d0
Proxy* checkProxy(lua_State* L, int index)
{
    return checkRootObject(L, index);
}

// 0x08156140
void disposeObject(lua_State* L, int index)
{
    Proxy* proxy = checkProxy(L, index);
    if (proxy->object)
    {
        lua_getfield(L, LUA_REGISTRYINDEX, "rapid.object_map");
        lua_pushlightuserdata(L, proxy->object);
        lua_pushnil(L);
        lua_rawset(L, -3);
        lua_pop(L, 1);
        if (proxy->destroy)
            proxy->destroy(proxy->object);
        proxy->object = 0;
    }
}

// checkObjectOpt<T>: the metatable or one of its bases must be the class table.
void* checkObjectOptRaw(lua_State* L, int index, const char* className)
{
    Proxy* proxy = (Proxy*)lua_touserdata(L, index);
    if (!proxy)
        return 0;
    lua_getfield(L, LUA_REGISTRYINDEX, className);
    lua_getmetatable(L, index);
    if (lua_type(L, -1) != LUA_TTABLE)
    {
        lua_pop(L, 2);
        return 0;
    }
    for (;;)
    {
        if (lua_rawequal(L, -1, -2))
        {
            lua_pop(L, 2);
            if (!proxy->object)
                luaL_argerror(L, index, "object has been disposed");
            return proxy->object;
        }
        lua_pushstring(L, "baseclass");
        lua_rawget(L, -2);
        if (lua_type(L, -1) != LUA_TTABLE)
        {
            lua_pop(L, 3);
            return 0;
        }
        lua_remove(L, -2);
    }
}

// ---- math values -----------------------------------------------------------------

static float tableNumber(lua_State* L, int index, int i)
{
    lua_rawgeti(L, index, i);
    float v = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);
    return v;
}
// 0x08155ca0
Vec2 checkVector2(lua_State* L, int index)
{
    if (lua_type(L, index) != LUA_TTABLE)
        luaL_typerror(L, index, "vec");
    return Vec2(tableNumber(L, index, 1), tableNumber(L, index, 2));
}
// 0x081557c0; Grimrock 2 (0x004114c0...) also takes three numbers wherever a vec is
// expected (Node.setPosition(x, y, z), RigidBody.addForce(x, y, z), ...)
Vec3 checkVector3(lua_State* L, int index)
{
#if GRIMROCK_GAME >= 2
    // only for the arguments of a binding, where the following stack slots hold the rest
    if (index > 0 && lua_isnumber(L, index))
        return Vec3((float)luaL_checknumber(L, index), (float)luaL_checknumber(L, index + 1),
                    (float)luaL_checknumber(L, index + 2));
#endif
    if (lua_type(L, index) != LUA_TTABLE)
        luaL_typerror(L, index, "vec");
    return Vec3(tableNumber(L, index, 1), tableNumber(L, index, 2), tableNumber(L, index, 3));
}
// 0x08155eb0: the loose form reads the following stack slots, so it is only valid for
// the arguments of a binding
Vec3 checkVector3_alt(lua_State* L, int index)
{
    if (index <= 0 || !lua_isnumber(L, index))
        return checkVector3(L, index);
    float x = (float)luaL_checknumber(L, index);
    float y = (float)luaL_checknumber(L, index + 1);
    float z = (float)luaL_checknumber(L, index + 2);
    return Vec3(x, y, z);
}
// 0x08155d70
Vec4 checkVector4(lua_State* L, int index)
{
    if (lua_type(L, index) != LUA_TTABLE)
        luaL_typerror(L, index, "vec");
    return Vec4(tableNumber(L, index, 1), tableNumber(L, index, 2), tableNumber(L, index, 3),
                tableNumber(L, index, 4));
}
#if GRIMROCK_GAME >= 2
// 0x00411150
Vec4 checkVector4_alt(lua_State* L, int index)
{
    if (index <= 0 || !lua_isnumber(L, index))
        return checkVector4(L, index);
    float x = (float)luaL_checknumber(L, index);
    float y = (float)luaL_checknumber(L, index + 1);
    float z = (float)luaL_checknumber(L, index + 2);
    float w = (float)luaL_checknumber(L, index + 3);
    return Vec4(x, y, z, w);
}
#endif
// 0x08155b00
Color checkColor(lua_State* L, int index)
{
#if GRIMROCK_GAME >= 2
    // 0x0040c370: the colour may also be a "RRGGBB" / "RRGGBBAA" string
    if (lua_isstring(L, index))
    {
        size_t length = 0;
        const char* hex = luaL_checklstring(L, index, &length);
        if (length != 6 && length != 8)
            luaL_typerror(L, index, "color");
        return Color::fromHex(hex);
    }
#endif
    if (lua_type(L, index) != LUA_TTABLE)
        luaL_typerror(L, index, "color");
    Color color;
    color.r = (unsigned char)(short)lrintf(tableNumber(L, index, 1));
    color.g = (unsigned char)(short)lrintf(tableNumber(L, index, 2));
    color.b = (unsigned char)(short)lrintf(tableNumber(L, index, 3));
    color.a = (unsigned char)(short)lrintf(tableNumber(L, index, 4));
    return color;
}
// 0x081582b0: {x=vec, y=vec, z=vec}
Matrix3x3 checkMatrix3x3(lua_State* L, int index)
{
    if (lua_type(L, index) != LUA_TTABLE)
        luaL_typerror(L, index, "mat");
    Matrix3x3 m;
    const char* axes[3] = {"x", "y", "z"};
    Vec3* cols[3] = {&m.x, &m.y, &m.z};
    for (int i = 0; i < 3; ++i)
    {
        lua_getfield(L, index, axes[i]);
        *cols[i] = checkVector3(L, -1);
        lua_pop(L, 1);
    }
    return m;
}
// 0x081580d0: {x=vec, y=vec, z=vec, w=vec}
Matrix4x3 checkMatrix4x3(lua_State* L, int index)
{
    if (lua_type(L, index) != LUA_TTABLE)
        luaL_typerror(L, index, "mat");
    Matrix4x3 m;
    const char* axes[4] = {"x", "y", "z", "w"};
    Vec3* cols[4] = {&m.x, &m.y, &m.z, &m.pos};
    for (int i = 0; i < 4; ++i)
    {
        lua_getfield(L, index, axes[i]);
        *cols[i] = checkVector3(L, -1);
        lua_pop(L, 1);
    }
    return m;
}
// Camera_setProjectionMatrix (0x08140710): {x={..4}, y={..4}, z={..4}, w={..4}}, the
// columns read with rawgeti.
Matrix4x4 checkMatrix4x4(lua_State* L, int index)
{
    if (lua_type(L, index) != LUA_TTABLE)
        luaL_typerror(L, index, "mat");
    Matrix4x4 m;
    const char* axes[4] = {"x", "y", "z", "w"};
    for (int i = 0; i < 4; ++i)
    {
        lua_getfield(L, index, axes[i]);
        if (lua_type(L, -1) != LUA_TTABLE)
            luaL_typerror(L, -1, "vec");
        for (int j = 0; j < 4; ++j)
        {
            lua_rawgeti(L, -1, j + 1);
            m.m[i * 4 + j] = (float)lua_tonumber(L, -1);
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }
    return m;
}
// 0x081556f0: vec(x, y, z, w)
void pushVector(lua_State* L, const Vec4& v)
{
    lua_getfield(L, LUA_GLOBALSINDEX, "vec");
    lua_pushnumber(L, v.x);
    lua_pushnumber(L, v.y);
    lua_pushnumber(L, v.z);
    lua_pushnumber(L, v.w);
    lua_call(L, 4, 1);
}
// 0x0815abf0: mat.create(vec, vec, vec, vec) with the rows of the matrix
void pushMatrix(lua_State* L, const Matrix4x4& m)
{
    lua_getfield(L, LUA_GLOBALSINDEX, "mat");
    lua_getfield(L, -1, "create");
    lua_remove(L, -2);
    for (int i = 0; i < 4; ++i)
        pushVector(L, Vec4(m.m[i * 4], m.m[i * 4 + 1], m.m[i * 4 + 2], m.m[i * 4 + 3]));
    lua_call(L, 4, 1);
}
void pushMatrix(lua_State* L, const Matrix4x3& m)
{
    lua_getfield(L, LUA_GLOBALSINDEX, "mat");
    lua_getfield(L, -1, "create");
    lua_remove(L, -2);
    pushVector(L, Vec4(m.x.x, m.x.y, m.x.z, 0.0f));
    pushVector(L, Vec4(m.y.x, m.y.y, m.y.z, 0.0f));
    pushVector(L, Vec4(m.z.x, m.z.y, m.z.z, 0.0f));
    pushVector(L, Vec4(m.pos.x, m.pos.y, m.pos.z, 1.0f));
    lua_call(L, 4, 1);
}
void pushMatrix(lua_State* L, const Matrix3x3& m)
{
    pushMatrix(L, Matrix4x3(m));
}
void pushColor(lua_State* L, const Color& c)
{
    lua_createtable(L, 4, 0);
    lua_pushnumber(L, c.r);
    lua_rawseti(L, -2, 1);
    lua_pushnumber(L, c.g);
    lua_rawseti(L, -2, 2);
    lua_pushnumber(L, c.b);
    lua_rawseti(L, -2, 3);
    lua_pushnumber(L, c.a);
    lua_rawseti(L, -2, 4);
}

// ---- loading ---------------------------------------------------------------------

namespace
{
struct FileChunk
{
    const char* data;
    size_t size;
};
// 0x0812a390
const char* fileReader(lua_State* L, void* ud, size_t* size)
{
    FileChunk* chunk = (FileChunk*)ud;
    const char* data = chunk->data;
    *size = chunk->size;
    chunk->size = 0;
    return data;
}
} // namespace

// 0x0812ba00
int load(lua_State* L, const char* filename)
{
    char* data = 0;
    try
    {
        int length = 0;
        data = readFile(filename, length);
        FileChunk chunk = {data, (size_t)length};
        String name = formatString("@%s", filename);
        int result = lua_load(L, fileReader, &chunk, name.c_str());
        delete[] data;
        return result;
    }
    catch (Exception& e)
    {
        delete[] data;
        lua_pushstring(L, e.getReason());
        return LUA_ERRFILE;
    }
}

} // namespace luax

namespace luax
{

// 0x08155770
bool checkBool(lua_State* L, int index)
{
    luaL_checktype(L, index, LUA_TBOOLEAN);
    return lua_toboolean(L, index) != 0;
}
// 0x081558c0
AABox3 checkBox(lua_State* L, int index)
{
    if (lua_type(L, index) != LUA_TTABLE)
        luaL_typerror(L, index, "box");
    lua_pushstring(L, "pos");
    lua_rawget(L, index);
    Vec3 pos = checkVector3(L, -1);
    lua_pop(L, 1);
    lua_pushstring(L, "hsize");
    lua_rawget(L, index);
    Vec3 hsize = checkVector3(L, -1);
    lua_pop(L, 1);
    return AABox3(pos - hsize, pos + hsize);
}
// 0x08155a00
Ray3 checkRay(lua_State* L, int index)
{
    if (lua_type(L, index) != LUA_TTABLE)
        luaL_typerror(L, index, "ray");
    lua_pushstring(L, "pos");
    lua_rawget(L, index);
    Vec3 pos = checkVector3(L, -1);
    lua_pop(L, 1);
    lua_pushstring(L, "dir");
    lua_rawget(L, index);
    Vec3 dir = checkVector3(L, -1);
    lua_pop(L, 1);
    Ray3 r;
    r.origin = pos;
    r.dir = dir;
    return r;
}
// RenderableMesh_getBoundingBox (0x08142e20): {pos=center, hsize=half extents}
void pushBox(lua_State* L, const AABox3& box)
{
    lua_createtable(L, 0, 0);
    pushVector(L, (box.min + box.max) * 0.5f);
    lua_setfield(L, -2, "pos");
    pushVector(L, (box.max - box.min) * 0.5f);
    lua_setfield(L, -2, "hsize");
}
// Camera_getViewRay (0x08146140): {pos=origin, dir=direction}
void pushRay(lua_State* L, const Ray3& ray)
{
    lua_createtable(L, 0, 0);
    pushVector(L, ray.origin);
    lua_setfield(L, -2, "pos");
    pushVector(L, ray.dir);
    lua_setfield(L, -2, "dir");
}

} // namespace luax
