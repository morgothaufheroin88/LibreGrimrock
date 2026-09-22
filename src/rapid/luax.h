// luax: helpers binding C++ objects to Lua, reconstructed from luax.cpp
// (0x0812a390-0x0812bb10) and the template instantiations in Engine.cpp/Core.cpp.
//
// Objects are 8 byte userdata proxies {void* object; void (*destroy)(void*)} whose
// metatable is the class table. Every class table has: type (name), baseclass,
// __index (itself or getProperty), properties (set of property names), and inherits
// the methods of its base. "rapid.object_map" maps object addresses to their proxies
// so the same C++ object always yields the same Lua value.
#pragma once
#include "core/Color.h"
#include "core/Matrix.h"
#include "core/SharedPtr.h"
#include "core/Vector.h"
extern "C"
{
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

namespace luax
{

struct Enum
{
    const char* name;
    int value;
};

// Proxy userdata layout (kept 2 pointers wide, both 64-bit safe).
struct Proxy
{
    void* object;
    void (*destroy)(void*);
};

// Name of the class table of T; specialised for every bound class.
template <class T> struct ClassName;
#define LUAX_CLASS(T, name)                                                                        \
    template <> struct luax::ClassName<T>                                                          \
    {                                                                                              \
        static const char* get()                                                                   \
        {                                                                                          \
            return name;                                                                           \
        }                                                                                          \
    };

void init(lua_State* L);
// Loads a chunk from the mounted file systems, chunk name "@filename".
int load(lua_State* L, const char* filename);
void dumpStack(lua_State* L);

void registerClass(lua_State* L, const char* name, const luaL_Reg* methods,
                   const char* const* properties);
void registerSubclass(lua_State* L, const char* name, const char* base, const luaL_Reg* methods,
                      const char* const* properties);
void registerModule(lua_State* L, const char* name, const luaL_Reg* functions);
void registerFunctions(lua_State* L, const luaL_Reg* functions);
void registerEnums(lua_State* L, const char* table, Enum* enums);
void setGlobal(lua_State* L, const char* name, const char* value);
void setGlobal(lua_State* L, const char* name, int value);
void createWeakTable(lua_State* L, const char* mode);
int checkEnum(lua_State* L, int index, const Enum* enums);
// Pushes the name of value, or "???" when it is not in the table.
void pushEnum(lua_State* L, int value, const Enum* enums);
// grimrock2.exe 0x0042f380: number of live proxies in rapid.object_map
int proxyCount(lua_State* L);

// Proxy access.
Proxy* checkProxy(lua_State* L, int index);
void pushObject(lua_State* L, void* object);
void disposeObject(lua_State* L, int index);
void createObjectRaw(lua_State* L, const char* className, void* object, void (*destroy)(void*));
void* checkObjectOptRaw(lua_State* L, int index, const char* className);

template <class T> void createObject(lua_State* L, T* object, void (*destroy)(void*))
{
    createObjectRaw(L, ClassName<T>::get(), object, destroy);
}
template <class T> T* checkObjectOpt(lua_State* L, int index)
{
    return (T*)checkObjectOptRaw(L, index, ClassName<T>::get());
}
template <class T> T* checkObject(lua_State* L, int index)
{
    T* p = checkObjectOpt<T>(L, index);
    if (!p)
        luaL_typerror(L, index, ClassName<T>::get());
    return p;
}

// Destructors for createObject: plain delete or a shared reference.
template <class T> struct ProxyDestructor
{
    static void destroy(void* p)
    {
        delete (T*)p;
    }
};
template <class T> struct SharedPtrDestructor
{
    static void destroy(void* p)
    {
        if (!p)
            return;
        int* rc = core::SharedPtrBase::refcounts().findValue(p);
        if (!rc)
            return;
        if (--*rc == 0)
        {
            core::SharedPtrBase::release(p);
            delete (T*)p;
        }
    }
};
// Proxy holding one shared reference.
template <class T> void createSharedObject(lua_State* L, T* object)
{
    ++*core::SharedPtrBase::acquire(object);
    createObject<T>(L, object, SharedPtrDestructor<T>::destroy);
}

// Math values are Lua tables: vectors {x,y,z,w} built with the global vec(), matrices
// with mat.create(), colours {r,g,b,a}.
core::Vec2 checkVector2(lua_State* L, int index);
core::Vec3 checkVector3(lua_State* L, int index);
// Accepts a vector table or three numbers starting at index.
core::Vec3 checkVector3_alt(lua_State* L, int index);
core::Vec4 checkVector4(lua_State* L, int index);
#if GRIMROCK_GAME >= 2
// 0x00411150: a vec or four numbers
core::Vec4 checkVector4_alt(lua_State* L, int index);
#endif
core::Color checkColor(lua_State* L, int index);
core::Matrix3x3 checkMatrix3x3(lua_State* L, int index);
core::Matrix4x3 checkMatrix4x3(lua_State* L, int index);
core::Matrix4x4 checkMatrix4x4(lua_State* L, int index);
void pushVector(lua_State* L, const core::Vec4& v);
inline void pushVector(lua_State* L, const core::Vec3& v)
{
    pushVector(L, core::Vec4(v.x, v.y, v.z, 0.0f));
}
inline void pushVector(lua_State* L, const core::Vec2& v)
{
    pushVector(L, core::Vec4(v.x, v.y, 0.0f, 0.0f));
}
void pushMatrix(lua_State* L, const core::Matrix4x4& m);
void pushMatrix(lua_State* L, const core::Matrix4x3& m);
void pushMatrix(lua_State* L, const core::Matrix3x3& m);
void pushColor(lua_State* L, const core::Color& c);
void pushUInt64(lua_State* L, unsigned long long v);
unsigned long long checkUInt64(lua_State* L, int index);
// Array of numbers from a table, new[]'ed.
template <class T> T* loadRawData(lua_State* L, int index)
{
    luaL_checktype(L, index, LUA_TTABLE);
    size_t n = lua_objlen(L, index);
    T* out = new T[n];
    for (size_t i = 0; i < n; ++i)
    {
        lua_rawgeti(L, index, (int)i + 1);
        out[i] = (T)lua_tonumber(L, -1);
        lua_pop(L, 1);
    }
    return out;
}

} // namespace luax

// ---- additions from Engine.cpp (0x08155770-0x08155a00) ----------------------------
#include "core/Prim.h"
namespace luax
{
bool checkBool(lua_State* L, int index);
// {pos=vec, hsize=vec}
core::AABox3 checkBox(lua_State* L, int index);
// {pos=vec, dir=vec}
core::Ray3 checkRay(lua_State* L, int index);
void pushBox(lua_State* L, const core::AABox3& box);
void pushRay(lua_State* L, const core::Ray3& ray);
} // namespace luax
