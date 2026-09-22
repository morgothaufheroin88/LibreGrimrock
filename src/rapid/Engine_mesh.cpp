// Mesh bindings of Engine.cpp (0x0814aef0-0x08151300).
#include "EngineBindings.h"
#include "core/Exception.h"

using namespace core;
using namespace engine;

// 0x082be780
static luax::Enum meshPrimTypes[] = {
    {"points", 0}, {"lines", 1}, {"triangles", 2}, {"quads", 3}, {0, 0}};

// 0x08145180: Mesh.create([numVertices])
static int Mesh_create(lua_State* L)
{
    int numVertices = luaL_optinteger(L, 1, 0);
    Mesh* mesh = new Mesh(numVertices);
    luax::createSharedObject<Mesh>(L, mesh);
    return 1;
}
// 0x08145070: Mesh.createPlane(x0, z0), the far corner is always (1, 1)
static int Mesh_createPlane(lua_State* L)
{
    float x0 = (float)luaL_checknumber(L, 1);
    float z0 = (float)luaL_checknumber(L, 2);
    luax::createSharedObject<Mesh>(L, createPlane(x0, z0, 1.0f, 1.0f));
    return 1;
}
// 0x08144fa0
static int Mesh_createTetrahedron(lua_State* L)
{
    luax::createSharedObject<Mesh>(L, createTetrahedron());
    return 1;
}
// 0x0814ac10: Mesh.createBox([width, height, depth])
static int Mesh_createBox(lua_State* L)
{
    Mesh* mesh;
    if (lua_gettop(L) == 3)
    {
        float width = (float)luaL_checknumber(L, 1);
        float height = (float)luaL_checknumber(L, 2);
        float depth = (float)luaL_checknumber(L, 3);
        mesh = createBox(width, height, depth);
    }
    else
    {
        if (lua_gettop(L) != 0)
            return luaL_error(L, "invalid number of args");
        mesh = createBox(2.0f, 2.0f, 2.0f);
    }
    luax::createSharedObject<Mesh>(L, mesh);
    return 1;
}
// 0x081454a0
static int Mesh_createSphere(lua_State* L)
{
    int segments = luaL_optinteger(L, 1, 4);
    luax::createSharedObject<Mesh>(L, createSphere(segments));
    return 1;
}
// 0x081453b0
static int Mesh_createCone(lua_State* L)
{
    int segments = luaL_optinteger(L, 1, 8);
    luax::createSharedObject<Mesh>(L, createCone(segments));
    return 1;
}
// 0x08145590
static int Mesh_load(lua_State* L)
{
    try
    {
        const char* filename = luaL_checkstring(L, 1);
        luax::createSharedObject<Mesh>(L, loadMesh(filename));
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x0814b0d0
static int Mesh_clone(lua_State* L)
{
    Mesh* mesh = luax::checkObject<Mesh>(L, 1);
    luax::createSharedObject<Mesh>(L, new Mesh(*mesh));
    return 1;
}
// 0x0814bb30
static int Mesh_setNumVertices(lua_State* L)
{
    Mesh* mesh = luax::checkObject<Mesh>(L, 1);
    mesh->setNumVertices(luaL_checkinteger(L, 2));
    return 0;
}
// 0x0814ba10-0x0814b470: the array is a flat table of numVertices * components values,
// nil clears the array.
template <class T> static int setVertexArray(lua_State* L, int index, int type, int components)
{
    Mesh* mesh = luax::checkObject<Mesh>(L, 1);
    if (lua_type(L, 2) == LUA_TNIL)
    {
        mesh->setVertexArray(index, type, components, 0);
        return 0;
    }
    int numVertices = mesh->getNumVertices();
    if ((int)lua_objlen(L, 2) != numVertices * components)
        luaL_argerror(L, 2, "invalid array size");
    T* data = luax::loadRawData<T>(L, 2);
    mesh->setVertexArray(index, type, components, data);
    delete[] data;
    return 0;
}
static int Mesh_setVertexArray(lua_State* L)
{
    return setVertexArray<float>(L, Mesh::Position, Mesh::TypeFloat, 3);
}
static int Mesh_setNormalArray(lua_State* L)
{
    return setVertexArray<float>(L, Mesh::Normal, Mesh::TypeFloat, 3);
}
static int Mesh_setTangentArray(lua_State* L)
{
    return setVertexArray<float>(L, Mesh::Tangent, Mesh::TypeFloat, 3);
}
static int Mesh_setBitangentArray(lua_State* L)
{
    return setVertexArray<float>(L, Mesh::Bitangent, Mesh::TypeFloat, 3);
}
static int Mesh_setColorArray(lua_State* L)
{
    return setVertexArray<unsigned char>(L, Mesh::Color, Mesh::TypeByte, 4);
}
static int Mesh_setTexcoordArray(lua_State* L)
{
    return setVertexArray<float>(L, Mesh::Texcoord0, Mesh::TypeFloat, 2);
}
// 0x0814b320
static int Mesh_setIndices(lua_State* L)
{
    Mesh* mesh = luax::checkObject<Mesh>(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    int count = (int)lua_objlen(L, 2);
    int numVertices = mesh->getNumVertices();
    int* indices = new int[count];
    for (int i = 0; i < count; ++i)
    {
        lua_rawgeti(L, 2, i + 1);
        int index = (int)(lua_tonumber(L, -1) + 0.5);
        indices[i] = index;
        if (index < 0 || index >= numVertices)
            luaL_error(L, "invalid vertex index");
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    mesh->setIndices(indices, count);
    delete[] indices;
    return 0;
}
// 0x0814c6a0
static int Mesh_clearSegments(lua_State* L)
{
    luax::checkObject<Mesh>(L, 1)->clearSegments();
    return 0;
}
// 0x0814c5b0: addSegment(material, primitiveType, firstIndex, count)
static int Mesh_addSegment(lua_State* L)
{
    Mesh* mesh = luax::checkObject<Mesh>(L, 1);
    MeshSegment& segment = mesh->addSegment();
    Material* material = luax::checkObject<Material>(L, 2);
    segment.material.reset(material);
    segment.primitiveType = luax::checkEnum(L, 3, meshPrimTypes);
    segment.firstIndex = luaL_checkinteger(L, 4);
    segment.numTriangles = luaL_checkinteger(L, 5);
    return 0;
}
// 0x0814c4c0-0x0814c010: flat table of the array values, nil when absent
template <class T> static int getVertexArray(lua_State* L, int index, int components)
{
    Mesh* mesh = luax::checkObject<Mesh>(L, 1);
    const T* data = (const T*)mesh->getVertexArray(index);
    if (data)
    {
        int n = mesh->getNumVertices() * components;
        lua_createtable(L, 0, 0);
        for (int i = 0; i < n; ++i)
        {
            lua_pushnumber(L, data[i]);
            lua_rawseti(L, -2, i + 1);
        }
        return 1;
    }
    lua_pushnil(L);
    return 1;
}
static int Mesh_getVertexArray(lua_State* L)
{
    return getVertexArray<float>(L, Mesh::Position, 3);
}
static int Mesh_getNormalArray(lua_State* L)
{
    return getVertexArray<float>(L, Mesh::Normal, 3);
}
static int Mesh_getTangentArray(lua_State* L)
{
    return getVertexArray<float>(L, Mesh::Tangent, 3);
}
static int Mesh_getBitangentArray(lua_State* L)
{
    return getVertexArray<float>(L, Mesh::Bitangent, 3);
}
static int Mesh_getColorArray(lua_State* L)
{
    return getVertexArray<unsigned char>(L, Mesh::Color, 4);
}
static int Mesh_getTexcoordArray(lua_State* L)
{
    return getVertexArray<float>(L, Mesh::Texcoord0, 2);
}
// 0x0814c770
static int Mesh_getNumVertices(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<Mesh>(L, 1)->getNumVertices());
    return 1;
}
// 0x0814c700
static int Mesh_getNumIndices(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<Mesh>(L, 1)->getIndices().size());
    return 1;
}
// 0x0814aef0
static int Mesh_getIndices(lua_State* L)
{
    Mesh* mesh = luax::checkObject<Mesh>(L, 1);
    const Array<int>& indices = mesh->getIndices();
    lua_createtable(L, 0, 0);
    for (int i = 0; i < indices.size(); ++i)
    {
        lua_pushnumber(L, indices[i]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}
// 0x0814c7e0
static int Mesh_getNumSegments(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<Mesh>(L, 1)->getNumSegments());
    return 1;
}
// 0x0814c850: {material=, firstIndex=, count=, type=}
static int Mesh_getSegment(lua_State* L)
{
    Mesh* mesh = luax::checkObject<Mesh>(L, 1);
    int index = luaL_checkinteger(L, 2);
    if (index < 1 || index > mesh->getNumSegments())
        luaL_error(L, "invalid mesh segment index");
    MeshSegment& segment = mesh->getSegment(index - 1);
    lua_createtable(L, 0, 0);
    pushSharedObject<Material>(L, segment.material.get());
    lua_setfield(L, -2, "material");
    lua_pushnumber(L, segment.firstIndex);
    lua_setfield(L, -2, "firstIndex");
    lua_pushnumber(L, segment.numTriangles);
    lua_setfield(L, -2, "count");
    const luax::Enum* e = meshPrimTypes;
    while (e->name && e->value != segment.primitiveType)
        ++e;
    lua_pushstring(L, e->name ? e->name : "???");
    lua_setfield(L, -2, "type");
    return 1;
}
// 0x0814ca50: {pos=, radius=}
static int Mesh_getBoundingSphere(lua_State* L)
{
    Mesh* mesh = luax::checkObject<Mesh>(L, 1);
    lua_createtable(L, 0, 0);
    luax::pushVector(L, mesh->getBoundingSphereCenter());
    lua_setfield(L, -2, "pos");
    lua_pushnumber(L, mesh->getBoundingSphereRadius());
    lua_setfield(L, -2, "radius");
    return 1;
}
// 0x081511a0
static int Mesh_getBoundingBox(lua_State* L)
{
    Mesh* mesh = luax::checkObject<Mesh>(L, 1);
    luax::pushBox(L, mesh->getBoundingBox());
    return 1;
}
// 0x0814ed00
static int Mesh_transform(lua_State* L)
{
    Mesh* mesh = luax::checkObject<Mesh>(L, 1);
    Matrix4x3 m = luax::checkMatrix4x3(L, 2);
    mesh->transform(m);
    return 0;
}
// 0x0814bf90
static int Mesh_translate(lua_State* L)
{
    Mesh* mesh = luax::checkObject<Mesh>(L, 1);
    Vec3 offset = luax::checkVector3_alt(L, 2);
    mesh->translate(offset);
    return 0;
}
// 0x0814bd90
static int Mesh_rotate(lua_State* L)
{
    Mesh* mesh = luax::checkObject<Mesh>(L, 1);
    Matrix3x3 m = luax::checkMatrix3x3(L, 2);
    mesh->rotate(m);
    return 0;
}
// 0x0814bd20
static int Mesh_scale(lua_State* L)
{
    Mesh* mesh = luax::checkObject<Mesh>(L, 1);
    mesh->scale((float)luaL_checknumber(L, 2));
    return 0;
}
// 0x0814bcc0
static int Mesh_computeVertexNormals(lua_State* L)
{
    luax::checkObject<Mesh>(L, 1)->computeVertexNormals();
    return 0;
}
// 0x0814bc60
static int Mesh_computeTangentVectors(lua_State* L)
{
    luax::checkObject<Mesh>(L, 1)->computeTangentVectors();
    return 0;
}
// 0x0814bc00
static int Mesh_weldVertices(lua_State* L)
{
    luax::checkObject<Mesh>(L, 1)->weldVertices();
    return 0;
}
#if GRIMROCK_GAME >= 2
// 0x00417170
static int Mesh_unweldVertices(lua_State* L)
{
    luax::checkObject<Mesh>(L, 1)->unweldVertices();
    return 0;
}
// 0x004171b0
static int Mesh_optimizeSegments(lua_State* L)
{
    luax::checkObject<Mesh>(L, 1)->optimizeSegments();
    return 0;
}
// 0x004171f0
static int Mesh_normalizeBoneWeights(lua_State* L)
{
    luax::checkObject<Mesh>(L, 1)->normalizeBoneWeights();
    return 0;
}
// 0x004172b0
static int Mesh_setMaterial(lua_State* L)
{
    Mesh* mesh = luax::checkObject<Mesh>(L, 1);
    mesh->setMaterial(luax::checkObject<Material>(L, 2));
    return 0;
}
// 0x00416250: saveFbx(filename [, scale]); the FBX SDK export of the editor is not
// reproduced
static int Mesh_saveFbx(lua_State* L)
{
    luax::checkObject<Mesh>(L, 1);
    luaL_checkstring(L, 2);
    return luaL_error(L, "FBX export is not supported");
}
#endif
// 0x0814bba0
static int Mesh_triangulate(lua_State* L)
{
    luax::checkObject<Mesh>(L, 1)->triangulate();
    return 0;
}
// 0x0814c9f0
static int Mesh_flipFaces(lua_State* L)
{
    luax::checkObject<Mesh>(L, 1)->flipFaces();
    return 0;
}
// 0x0814d620: raycast(ray, anyHit) -> hit [, t, triangle, normal]
static int Mesh_raycast(lua_State* L)
{
    Mesh* mesh = luax::checkObject<Mesh>(L, 1);
    Ray3 ray = luax::checkRay(L, 2);
    bool anyHit = luax::checkBool(L, 3);
    if (!anyHit)
    {
        float distance;
        int triangle;
        Vec3 normal(0, 0, 0);
        bool hit = mesh->raycast(ray, distance, &triangle, &normal);
        lua_pushboolean(L, hit);
        lua_pushnumber(L, distance);
        lua_pushnumber(L, triangle);
        luax::pushVector(L, normal);
        return 4;
    }
    lua_pushboolean(L, mesh->raycast(ray));
    return 1;
}

const luaL_Reg Mesh_methods[] = {{"create", Mesh_create},
                                 {"createPlane", Mesh_createPlane},
                                 {"createTetrahedron", Mesh_createTetrahedron},
                                 {"createBox", Mesh_createBox},
                                 {"createSphere", Mesh_createSphere},
                                 {"createCone", Mesh_createCone},
                                 {"load", Mesh_load},
#if GRIMROCK_GAME >= 2
    {"saveFbx", Mesh_saveFbx},
#endif
                                 {"clone", Mesh_clone},
                                 {"setNumVertices", Mesh_setNumVertices},
                                 {"setVertexArray", Mesh_setVertexArray},
                                 {"setNormalArray", Mesh_setNormalArray},
                                 {"setTangentArray", Mesh_setTangentArray},
                                 {"setBitangentArray", Mesh_setBitangentArray},
                                 {"setColorArray", Mesh_setColorArray},
                                 {"setTexcoordArray", Mesh_setTexcoordArray},
                                 {"setIndices", Mesh_setIndices},
                                 {"clearSegments", Mesh_clearSegments},
                                 {"addSegment", Mesh_addSegment},
                                 {"getVertexArray", Mesh_getVertexArray},
                                 {"getNormalArray", Mesh_getNormalArray},
                                 {"getTangentArray", Mesh_getTangentArray},
                                 {"getBitangentArray", Mesh_getBitangentArray},
                                 {"getColorArray", Mesh_getColorArray},
                                 {"getTexcoordArray", Mesh_getTexcoordArray},
                                 {"getNumVertices", Mesh_getNumVertices},
                                 {"getNumIndices", Mesh_getNumIndices},
                                 {"getIndices", Mesh_getIndices},
                                 {"getNumSegments", Mesh_getNumSegments},
                                 {"getSegment", Mesh_getSegment},
                                 {"getBoundingSphere", Mesh_getBoundingSphere},
                                 {"getBoundingBox", Mesh_getBoundingBox},
                                 {"transform", Mesh_transform},
                                 {"translate", Mesh_translate},
                                 {"rotate", Mesh_rotate},
                                 {"scale", Mesh_scale},
                                 {"computeVertexNormals", Mesh_computeVertexNormals},
                                 {"computeTangentVectors", Mesh_computeTangentVectors},
                                 {"weldVertices", Mesh_weldVertices},
#if GRIMROCK_GAME >= 2
    {"unweldVertices", Mesh_unweldVertices},
    {"optimizeSegments", Mesh_optimizeSegments},
    {"normalizeBoneWeights", Mesh_normalizeBoneWeights},
#endif
                                 {"triangulate", Mesh_triangulate},
                                 {"flipFaces", Mesh_flipFaces},
#if GRIMROCK_GAME >= 2
    {"setMaterial", Mesh_setMaterial},
#endif
                                 {"raycast", Mesh_raycast},
                                 {0, 0}};
