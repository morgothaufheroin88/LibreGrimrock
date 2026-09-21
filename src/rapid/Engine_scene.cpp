// EngineSystems, Scene and Node bindings of Engine.cpp (0x0813ec40-0x08152910).
#include "EngineBindings.h"
#include "Frame.h"
#include "core/Exception.h"
#include <cstring>

using namespace core;
using namespace engine;

EngineSystems* g_pEngineSystems = 0;

luax::Enum g_renderEngines[] = {{"d3d9", 0}, {"gles2", 1}, {"opengl", 2}, {0, 0}};
luax::Enum g_lightTypes[] = {{"ambient", 0}, {"directional", 1}, {"point", 2}, {"spot", 3}, {0, 0}};
luax::Enum g_physicsEngines[] = {{"physx", 1}, {"null", 0}, {0, 0}};
luax::Enum g_audioEngines[] = {{"fmod", 1}, {"openal", 2}, {"xaudio2", 3}, {"null", 0}, {0, 0}};

// 0x08138ea0
void shutdownEngine()
{
    delete g_pEngineSystems;
    g_pEngineSystems = 0;
}

// 0x0813bc60
void pushNode(lua_State* L, Node* node)
{
    if (!node)
    {
        lua_pushnil(L);
        return;
    }
    luax::pushObject(L, node);
    if (lua_isnil(L, -1))
    {
        lua_pop(L, 1);
        luax::createObject<Node>(L, node, 0);
    }
}

// 0x08150c40
void disposeNodes(lua_State* L, Node* node)
{
    for (Node* child = node->getFirstChild(); child; child = child->getNextSibling())
        disposeNodes(L, child);
    luax::pushObject(L, node);
    if (!lua_isnil(L, -1))
        luax::disposeObject(L, -1);
    lua_pop(L, 1);
}

// Node_getRenderEntity (0x08148960)
void pushRenderEntity(lua_State* L, RenderEntity* entity)
{
    if (!entity)
    {
        lua_pushnil(L);
        return;
    }
    switch (entity->getEntityType())
    {
    case RenderEntity::MeshEntityType:
        pushSharedObject<MeshEntity>(L, (MeshEntity*)entity);
        break;
    case RenderEntity::LightEntityType:
        pushSharedObject<LightEntity>(L, (LightEntity*)entity);
        break;
    case RenderEntity::ParticleEntityType:
        pushSharedObject<ParticleEntity>(L, (ParticleEntity*)entity);
        break;
    default:
        luaL_error(L, "unknown render entity type");
    }
}

// ---- EngineSystems ---------------------------------------------------------------

// 0x081514c0: EngineSystems.create{width=, height=, windowed=, verticalSync=, ...}
static int EngineSystems_create(lua_State* L)
{
    try
    {
        if (g_pEngineSystems)
            luaL_error(L, "already created");
        RendererConfig config;
        if (g_pMainFrame)
            config.window = g_pMainFrame->getWindow();
        int physicsEngine = 0, audioEngine = 0;
        if (lua_gettop(L) > 0)
        {
            luaL_checktype(L, 1, LUA_TTABLE);
            lua_getfield(L, 1, "width");
            if (!lua_isnil(L, -1))
                config.width = luaL_checkinteger(L, -1);
            lua_getfield(L, 1, "height");
            if (!lua_isnil(L, -1))
                config.height = luaL_checkinteger(L, -1);
            lua_getfield(L, 1, "windowed");
            if (!lua_isnil(L, -1))
                config.windowed = luax::checkBool(L, -1);
            lua_getfield(L, 1, "verticalSync");
            if (!lua_isnil(L, -1))
            {
                if (lua_type(L, -1) == LUA_TBOOLEAN)
                {
                    config.verticalSync = luax::checkBool(L, -1) ? 1 : 0;
                }
                else
                {
                    const char* mode = luaL_checkstring(L, -1);
                    if (strcmp(mode, "triple_buffer") != 0)
                        return luaL_error(L, "invalid vertical sync mode");
                    config.verticalSync = 2;
                }
            }
            lua_getfield(L, 1, "multisamples");
            if (!lua_isnil(L, -1))
                config.multisamples = luaL_checkinteger(L, -1);
            lua_getfield(L, 1, "altShaderPath");
            if (!lua_isnil(L, -1))
                config.altShaderPath = luaL_checkstring(L, -1);
            lua_getfield(L, 1, "notebookMode");
            if (!lua_isnil(L, -1))
                config.notebookMode = luax::checkBool(L, -1);
            lua_getfield(L, 1, "dontAdjustFrame");
            if (!lua_isnil(L, -1))
                luax::checkBool(L, -1);
            lua_getfield(L, 1, "renderEngine");
            if (!lua_isnil(L, -1))
                config.renderEngine = luax::checkEnum(L, -1, g_renderEngines);
            lua_getfield(L, 1, "physicsEngine");
            if (!lua_isnil(L, -1))
                physicsEngine = luax::checkEnum(L, -1, g_physicsEngines);
            lua_getfield(L, 1, "audioEngine");
            if (!lua_isnil(L, -1))
                audioEngine = luax::checkEnum(L, -1, g_audioEngines);
        }
        g_pEngineSystems = new EngineSystems(config, physicsEngine, audioEngine);
        luax::createObject<EngineSystems>(L, g_pEngineSystems, 0);
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}
// 0x0813cbb0
static int EngineSystems_configureAssetPipeline(lua_State* L)
{
    static luax::Enum pipelines[] = {{"gles2", 0}, {0, 0}};
    EngineSystems* systems = luax::checkObject<EngineSystems>(L, 1);
    int pipeline = luax::checkEnum(L, 2, pipelines);
    systems->configureAssetPipeline(pipeline);
    return 0;
}

const luaL_Reg EngineSystems_methods[] = {
    {"create", EngineSystems_create},
    {"configureAssetPipeline", EngineSystems_configureAssetPipeline},
    {0, 0}};

// ---- Scene -----------------------------------------------------------------------

namespace
{
// Disposes the proxies of every node when the scene goes away (Scene_dispose).
struct NodeDeleter : NodeVisitor
{
    lua_State* L;
    void visit(Node* node)
    {
        luax::pushObject(L, node);
        if (!lua_isnil(L, -1))
            luax::disposeObject(L, -1);
        lua_pop(L, 1);
    }
};
// Appends the visited nodes to the table on top of the stack (Scene_query).
struct NodeCollector : NodeVisitor
{
    lua_State* L;
    int count;
    void visit(Node* node)
    {
        pushNode(L, node);
        lua_rawseti(L, -2, ++count);
    }
};
} // namespace

// 0x0814cf70
static int Scene_dispose(lua_State* L)
{
    luax::Proxy* p = luax::checkProxy(L, 1);
    if (p->object)
    {
        NodeDeleter deleter;
        deleter.L = L;
        ((Scene*)p->object)->query(deleter);
        luax::disposeObject(L, 1);
    }
    return 0;
}
// 0x08143190
static int Scene_createSimpleScene(lua_State* L)
{
    luax::createSharedObject<Scene>(L, Scene::createSimpleScene());
    return 1;
}
// 0x081456b0
static int Scene_createQuadTreeScene(lua_State* L)
{
    float size = (float)luaL_checknumber(L, 1);
    int levels = luaL_checkinteger(L, 2);
    luax::createSharedObject<Scene>(L, Scene::createQuadTreeScene(size, levels));
    return 1;
}
// 0x0813ec40
static int Scene_addNode(lua_State* L)
{
    Scene* scene = luax::checkObject<Scene>(L, 1);
    luax::createObject<Node>(L, scene->addNode(), 0);
    return 1;
}
// 0x08151340: removeNode(node, recursive)
static int Scene_removeNode(lua_State* L)
{
    Scene* scene = luax::checkObject<Scene>(L, 1);
    Node* node = luax::checkObject<Node>(L, 2);
    bool recursive = false;
    if (lua_type(L, 3) != LUA_TNONE)
    {
        luaL_checktype(L, 3, LUA_TBOOLEAN);
        recursive = lua_toboolean(L, 3) != 0;
    }
    if (recursive)
        disposeNodes(L, node);
    else
        luax::disposeObject(L, 2);
    scene->removeNode(*node, recursive);
    return 0;
}
// 0x081526c0
static int Scene_findNode(lua_State* L)
{
    Scene* scene = luax::checkObject<Scene>(L, 1);
    const char* name = luaL_checkstring(L, 2);
    pushNode(L, scene->findNode(name));
    return 1;
}
// 0x0813ecb0: query("all" | "box", box | "sphere", {pos, radius} | "ray", ray)
static int Scene_query(lua_State* L)
{
    Scene* scene = luax::checkObject<Scene>(L, 1);
    const char* type = luaL_checkstring(L, 2);
    NodeCollector collector;
    collector.L = L;
    collector.count = 0;
    if (strcmp(type, "all") == 0)
    {
        lua_newtable(L);
        scene->query(collector);
    }
    else if (strcmp(type, "box") == 0)
    {
        AABox3 box = luax::checkBox(L, 3);
        lua_newtable(L);
        scene->query(box, collector);
    }
    else if (strcmp(type, "sphere") == 0)
    {
        if (lua_type(L, 3) != LUA_TTABLE)
            luaL_typerror(L, 3, "sphere");
        lua_pushstring(L, "pos");
        lua_rawget(L, 3);
        Vec3 pos = luax::checkVector3(L, -1);
        lua_pop(L, 1);
        lua_pushstring(L, "radius");
        lua_rawget(L, 3);
        float radius = (float)luaL_checknumber(L, -1);
        lua_pop(L, 1);
        lua_newtable(L);
        scene->query(Sphere3(pos, radius), collector);
    }
    else if (strcmp(type, "ray") == 0)
    {
        Ray3 ray = luax::checkRay(L, 3);
        lua_newtable(L);
        scene->query(ray, collector);
    }
    else
    {
        luaL_argerror(L, 2, "invalid query type");
    }
    return 1;
}
// 0x081527a0: createMesh(mesh | renderableMesh)
static int Scene_createMesh(lua_State* L)
{
    Scene* scene = luax::checkObject<Scene>(L, 1);
    RenderableMesh* renderable;
    Mesh* mesh = luax::checkObjectOpt<Mesh>(L, 2);
    if (!mesh)
    {
        renderable = luax::checkObjectOpt<RenderableMesh>(L, 2);
    }
    else
    {
        if (mesh->getIndices().size() == 0)
            luaL_error(L, "mesh does not have indices");
        renderable = createRenderableMesh(*mesh, false);
    }
    Node* node = scene->addNode();
    MeshEntity* entity = new MeshEntity(renderable);
    pushNode(L, node);
    node->setRenderEntity(entity);
    return 1;
}
// 0x08152240: createLight(type, color[, dir | pos, range[, spotAngle]])
static int Scene_createLight(lua_State* L)
{
    Scene* scene = luax::checkObject<Scene>(L, 1);
    int type = luax::checkEnum(L, 2, g_lightTypes);
    Node* node = scene->addNode();
    LightEntity* light = new LightEntity((LightEntity::LightType)type);
    node->setRenderEntity(light);
    switch (type)
    {
    case LightEntity::Ambient:
        light->setLightColor(luax::checkVector3(L, 3));
        break;
    case LightEntity::Directional:
    {
        light->setLightColor(luax::checkVector3(L, 3));
        Vec3 dir = luax::checkVector3(L, 4);
        node->lookAt(normalize(dir));
        break;
    }
    case LightEntity::Point:
        light->setLightColor(luax::checkVector3(L, 3));
        node->setPosition(luax::checkVector3(L, 4));
        light->setLightRange((float)luaL_checknumber(L, 5));
        break;
    case LightEntity::Spot:
    {
        light->setLightColor(luax::checkVector3(L, 3));
        Vec3 dir = luax::checkVector3(L, 4);
        node->lookAt(normalize(dir));
        light->setLightRange((float)luaL_checknumber(L, 5));
        light->setSpotAngle((float)luaL_checknumber(L, 6));
        break;
    }
    default:
        luaL_error(L, "unknown light type");
    }
    pushNode(L, node);
    return 1;
}

const luaL_Reg Scene_methods[] = {{"__gc", Scene_dispose},
                                  {"dispose", Scene_dispose},
                                  {"createSimpleScene", Scene_createSimpleScene},
                                  {"createQuadTreeScene", Scene_createQuadTreeScene},
                                  {"addNode", Scene_addNode},
                                  {"removeNode", Scene_removeNode},
                                  {"findNode", Scene_findNode},
                                  {"query", Scene_query},
                                  {"createMesh", Scene_createMesh},
                                  {"createLight", Scene_createLight},
                                  {0, 0}};

// ---- Node ------------------------------------------------------------------------

static int Node_addTo(lua_State* L)
{
    Node* node = luax::checkObject<Node>(L, 1);
    Node* parent = luax::checkObject<Node>(L, 2);
    node->addTo(parent);
    return 1;
}
static int Node_remove(lua_State* L)
{
    luax::checkObject<Node>(L, 1)->remove();
    return 1;
}
static int Node_getParent(lua_State* L)
{
    Node* node = luax::checkObject<Node>(L, 1);
    pushNode(L, node->getParent());
    return 1;
}
// 0x08150790
static int Node_getChildren(lua_State* L)
{
    Node* node = luax::checkObject<Node>(L, 1);
    lua_newtable(L);
    int i = 1;
    for (Node* child = node->getFirstChild(); child; child = child->getNextSibling())
    {
        pushNode(L, child);
        if (lua_isnil(L, -1))
            lua_pop(L, 1);
        else
            lua_rawseti(L, -2, i++);
    }
    return 1;
}
static int Node_findNode(lua_State* L)
{
    Node* node = luax::checkObject<Node>(L, 1);
    pushNode(L, node->findNode(luaL_checkstring(L, 2)));
    return 1;
}
static int Node_setName(lua_State* L)
{
    luax::checkObject<Node>(L, 1)->setName(luaL_checkstring(L, 2));
    return 0;
}
static int Node_setPosition(lua_State* L)
{
    Node* node = luax::checkObject<Node>(L, 1);
    node->setPosition(luax::checkVector3_alt(L, 2));
    return 0;
}
// 0x081483b0: setRotation(matrix) or setRotation(rx, ry, rz[, order])
static int Node_setRotation(lua_State* L)
{
    static luax::Enum orders[] = {{"xyz", 0}, {"xzy", 1}, {"yxz", 2}, {"yzx", 3},
                                  {"zxy", 4}, {"zyx", 5}, {0, 0}};
    Node* node = luax::checkObject<Node>(L, 1);
    if (lua_gettop(L) == 2)
    {
        node->setRotation(luax::checkMatrix3x3(L, 2));
        return 0;
    }
    float rx = (float)luaL_checknumber(L, 2);
    float ry = (float)luaL_checknumber(L, 3);
    float rz = (float)luaL_checknumber(L, 4);
    const char* orderName = luaL_optstring(L, 5, "xyz");
    int order = -1;
    for (int i = 0; orders[i].name; ++i)
        if (strcmp(orderName, orders[i].name) == 0)
            order = orders[i].value;
    if (order < 0)
        luaL_argerror(L, 5, "invalid rotation order");
    Matrix3x3 m;
    m.makeRotation(rx, ry, rz, (Matrix3x3::Order)order);
    node->setRotation(m);
    return 0;
}
static int Node_setTransform(lua_State* L)
{
    Node* node = luax::checkObject<Node>(L, 1);
    node->setLocalMatrix(luax::checkMatrix4x3(L, 2));
    return 0;
}
static int Node_getName(lua_State* L)
{
    lua_pushstring(L, luax::checkObject<Node>(L, 1)->getName().c_str());
    return 1;
}
static int Node_getPosition(lua_State* L)
{
    luax::pushVector(L, luax::checkObject<Node>(L, 1)->getPosition());
    return 1;
}
static int Node_getRotation(lua_State* L)
{
    luax::pushMatrix(L, luax::checkObject<Node>(L, 1)->getRotation());
    return 1;
}
static int Node_getTransform(lua_State* L)
{
    luax::pushMatrix(L, luax::checkObject<Node>(L, 1)->getLocalMatrix());
    return 1;
}
static int Node_getWorldPosition(lua_State* L)
{
    luax::pushVector(L, luax::checkObject<Node>(L, 1)->getLocalToWorldMatrix().pos);
    return 1;
}
static int Node_getWorldRotation(lua_State* L)
{
    luax::pushMatrix(L, luax::checkObject<Node>(L, 1)->getLocalToWorldMatrix().rotation());
    return 1;
}
static int Node_getLocalToWorldMatrix(lua_State* L)
{
    luax::pushMatrix(L, luax::checkObject<Node>(L, 1)->getLocalToWorldMatrix());
    return 1;
}
static int Node_getWorldToLocalMatrix(lua_State* L)
{
    luax::pushMatrix(L, luax::checkObject<Node>(L, 1)->getWorldToLocalMatrix());
    return 1;
}
static int Node_move(lua_State* L)
{
    Node* node = luax::checkObject<Node>(L, 1);
    node->move(luax::checkVector3_alt(L, 2));
    return 0;
}
static int Node_rotate(lua_State* L)
{
    Node* node = luax::checkObject<Node>(L, 1);
    node->rotate(luax::checkMatrix3x3(L, 2));
    return 0;
}
// 0x08149010: rotateAbout(axis 0..2, angle)
static int Node_rotateAbout(lua_State* L)
{
    Node* node = luax::checkObject<Node>(L, 1);
    int axis = luaL_checkinteger(L, 2);
    if (axis < 0 || axis > 2)
        luaL_error(L, "invalid base vector index");
    node->rotateAbout(axis, (float)luaL_checknumber(L, 3));
    return 0;
}
static int Node_transform(lua_State* L)
{
    Node* node = luax::checkObject<Node>(L, 1);
    node->transform(luax::checkMatrix4x3(L, 2));
    return 0;
}
static int Node_lookAt(lua_State* L)
{
    Node* node = luax::checkObject<Node>(L, 1);
    node->lookAt(luax::checkVector3_alt(L, 2));
    return 0;
}
static int Node_setRenderEntity(lua_State* L)
{
    Node* node = luax::checkObject<Node>(L, 1);
    if (lua_isnil(L, 2))
        node->setRenderEntity(0);
    else
        node->setRenderEntity(luax::checkObject<RenderEntity>(L, 2));
    return 0;
}
static int Node_setRigidBody(lua_State* L)
{
    Node* node = luax::checkObject<Node>(L, 1);
    if (lua_isnil(L, 2))
        node->setRigidBody(0);
    else
        node->setRigidBody(luax::checkObject<RigidBody>(L, 2));
    return 0;
}
static int Node_setSoundSource(lua_State* L)
{
    Node* node = luax::checkObject<Node>(L, 1);
    if (lua_isnil(L, 2))
        node->setSoundSource(0);
    else
        node->setSoundSource(luax::checkObject<SoundSource>(L, 2));
    return 0;
}
static int Node_getRenderEntity(lua_State* L)
{
    Node* node = luax::checkObject<Node>(L, 1);
    pushRenderEntity(L, node->getRenderEntity());
    return 1;
}
static int Node_getRigidBody(lua_State* L)
{
    Node* node = luax::checkObject<Node>(L, 1);
    pushSharedObject<RigidBody>(L, node->getRigidBody());
    return 1;
}
static int Node_getSoundSource(lua_State* L)
{
    Node* node = luax::checkObject<Node>(L, 1);
    pushSharedObject<SoundSource>(L, node->getSoundSource());
    return 1;
}

const luaL_Reg Node_methods[] = {{"addTo", Node_addTo},
                                 {"remove", Node_remove},
                                 {"getParent", Node_getParent},
                                 {"getChildren", Node_getChildren},
                                 {"findNode", Node_findNode},
                                 {"setName", Node_setName},
                                 {"setPosition", Node_setPosition},
                                 {"setRotation", Node_setRotation},
                                 {"setTransform", Node_setTransform},
                                 {"getName", Node_getName},
                                 {"getPosition", Node_getPosition},
                                 {"getRotation", Node_getRotation},
                                 {"getTransform", Node_getTransform},
                                 {"getWorldPosition", Node_getWorldPosition},
                                 {"getWorldRotation", Node_getWorldRotation},
                                 {"getLocalToWorldMatrix", Node_getLocalToWorldMatrix},
                                 {"getWorldToLocalMatrix", Node_getWorldToLocalMatrix},
                                 {"move", Node_move},
                                 {"rotate", Node_rotate},
                                 {"rotateAbout", Node_rotateAbout},
                                 {"transform", Node_transform},
                                 {"lookAt", Node_lookAt},
                                 {"setRenderEntity", Node_setRenderEntity},
                                 {"setRigidBody", Node_setRigidBody},
                                 {"setSoundSource", Node_setSoundSource},
                                 {"getRenderEntity", Node_getRenderEntity},
                                 {"getRigidBody", Node_getRigidBody},
                                 {"getSoundSource", Node_getSoundSource},
                                 {0, 0}};
const char* Node_properties[] = {"Name",         "Position",  "Rotation",    "Transform",
                                 "RenderEntity", "RigidBody", "SoundSource", 0};
