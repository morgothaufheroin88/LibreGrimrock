// Physics bindings of Engine.cpp: PhysicsEngine, DynamicsWorld, CollisionMesh,
// CollisionShape, RigidBody, PhysicsMaterial, CharacterController.
#include "EngineBindings.h"
#include "core/Exception.h"
#include <cstring>

using namespace core;
using namespace engine;

// ---- PhysicsEngine ---------------------------------------------------------------

// 0x081458f0
static int PhysicsEngine_active(lua_State* L)
{
    if (!g_pEngineSystems)
        luaL_error(L, "engine not initialized");
    pushSharedObject<PhysicsEngine>(L, PhysicsEngine::sm_pActivePhysicsEngine);
    return 1;
}

const luaL_Reg PhysicsEngine_methods[] = {{"active", PhysicsEngine_active}, {0, 0}};

// ---- DynamicsWorld ---------------------------------------------------------------

// 0x08144440
static int DynamicsWorld_create(lua_State* L)
{
    DynamicsWorld* world = PhysicsEngine::sm_pActivePhysicsEngine->createWorld();
    luax::createSharedObject<DynamicsWorld>(L, world);
    return 1;
}
// 0x0813ebc0
static int DynamicsWorld_setGravity(lua_State* L)
{
    DynamicsWorld* world = luax::checkObject<DynamicsWorld>(L, 1);
    world->setGravity(luax::checkVector3_alt(L, 2));
    return 0;
}
// 0x0814d460: enableCollisions(group1, group2, enable)
static int DynamicsWorld_enableCollisions(lua_State* L)
{
    DynamicsWorld* world = luax::checkObject<DynamicsWorld>(L, 1);
    int group1 = luaL_checkinteger(L, 2);
    int group2 = luaL_checkinteger(L, 3);
    bool enable = luax::checkBool(L, 4);
    if (group1 == group2)
        luaL_error(L, "group1 must not be same as group2");
    world->enableCollisions(group1, group2, enable);
    return 0;
}
// 0x0814d370
static int DynamicsWorld_enableContactReporting(lua_State* L)
{
    DynamicsWorld* world = luax::checkObject<DynamicsWorld>(L, 1);
    int group1 = luaL_checkinteger(L, 2);
    int group2 = luaL_checkinteger(L, 3);
    bool enable = luax::checkBool(L, 4);
    if (group1 == group2)
        luaL_error(L, "group1 must not be same as group2");
    world->enableContactReporting(group1, group2, enable);
    return 0;
}
// 0x0814aa80: raycastAnyShape(ray, maxDistance [, groups])
static int DynamicsWorld_raycastAnyShape(lua_State* L)
{
    DynamicsWorld* world = luax::checkObject<DynamicsWorld>(L, 1);
    Ray3 ray = luax::checkRay(L, 2);
    float maxDistance = (float)luaL_checknumber(L, 3);
    int groups = luaL_optinteger(L, 4, -1);
    lua_pushboolean(L, world->raycastAnyShape(ray, maxDistance, groups));
    return 1;
}
// 0x0813d7f0: {node1=, node2=, sumNormalForce=} or nil
static int DynamicsWorld_getContactReport(lua_State* L)
{
    DynamicsWorld* world = luax::checkObject<DynamicsWorld>(L, 1);
    const ContactReport* report = world->getContactReport();
    if (report)
    {
        lua_createtable(L, 0, 0);
        pushNode(L, report->node1);
        lua_setfield(L, -2, "node1");
        pushNode(L, report->node2);
        lua_setfield(L, -2, "node2");
        luax::pushVector(L, report->sumNormalForce);
        lua_setfield(L, -2, "sumNormalForce");
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

const luaL_Reg DynamicsWorld_methods[] = {
    {"create", DynamicsWorld_create},
    {"setGravity", DynamicsWorld_setGravity},
    {"enableCollisions", DynamicsWorld_enableCollisions},
    {"enableContactReporting", DynamicsWorld_enableContactReporting},
    {"raycastAnyShape", DynamicsWorld_raycastAnyShape},
    {"getContactReport", DynamicsWorld_getContactReport},
    {0, 0}};

// ---- CollisionMesh ---------------------------------------------------------------

// 0x0814b200
static int CollisionMesh_create(lua_State* L)
{
    Mesh* mesh = luax::checkObject<Mesh>(L, 1);
    luax::createSharedObject<CollisionMesh>(L, createCollisionMesh(*mesh, false));
    return 1;
}
// 0x08145290
static int CollisionMesh_load(lua_State* L)
{
    try
    {
        const char* filename = luaL_checkstring(L, 1);
        luax::createSharedObject<CollisionMesh>(L, loadCollisionMesh(filename));
        return 1;
    }
    catch (core::Exception& e)
    {
        return luaL_error(L, "%s", e.getReason());
    }
}

const luaL_Reg CollisionMesh_methods[] = {
    {"create", CollisionMesh_create}, {"load", CollisionMesh_load}, {0, 0}};

// ---- CollisionShape --------------------------------------------------------------

// 0x08144320: createPlane(normal, d)
static int CollisionShape_createPlane(lua_State* L)
{
    Vec3 normal = luax::checkVector3(L, 1);
    float distance = (float)luaL_checknumber(L, 2);
    luax::createSharedObject<PlaneShape>(L, new PlaneShape(normal, distance));
    return 1;
}
// 0x08144210: createBox(halfExtents)
static int CollisionShape_createBox(lua_State* L)
{
    Vec3 halfExtents = luax::checkVector3(L, 1);
    luax::createSharedObject<BoxShape>(L, new BoxShape(halfExtents));
    return 1;
}
// 0x08144110: createSphere(radius)
static int CollisionShape_createSphere(lua_State* L)
{
    float radius = (float)luaL_checknumber(L, 1);
    luax::createSharedObject<SphereShape>(L, new SphereShape(radius));
    return 1;
}
// 0x08143ff0: createMesh(collisionMesh)
static int CollisionShape_createMesh(lua_State* L)
{
    CollisionMesh* mesh = luax::checkObject<CollisionMesh>(L, 1);
    luax::createSharedObject<MeshShape>(L, new MeshShape(mesh));
    return 1;
}

const luaL_Reg CollisionShape_methods[] = {{"createPlane", CollisionShape_createPlane},
                                           {"createBox", CollisionShape_createBox},
                                           {"createSphere", CollisionShape_createSphere},
                                           {"createMesh", CollisionShape_createMesh},
                                           {0, 0}};

// ---- RigidBody -------------------------------------------------------------------

// 0x08146a10: RigidBody.create(world, shape, "static"|"dynamic", mass, group)
static int RigidBody_create(lua_State* L)
{
    DynamicsWorld* world = luax::checkObject<DynamicsWorld>(L, 1);
    CollisionShape* shape = luax::checkObject<CollisionShape>(L, 2);
    BodyDesc desc;
    const char* type = luaL_checkstring(L, 3);
    if (strcmp(type, "static") == 0)
        desc.type = BodyDesc::Static;
    else if (strcmp(type, "dynamic") == 0)
        desc.type = BodyDesc::Dynamic;
    else
        luaL_argerror(L, 3, "invalid body type");
    float mass = (float)luaL_checknumber(L, 4);
    desc.mass = mass;
    if (desc.type == BodyDesc::Dynamic && mass <= 0.0f)
        luaL_argerror(L, 4, "mass must be positive");
    desc.group = luaL_checkinteger(L, 5);
    RigidBody* body = world->createBody(*shape, desc);
    luax::createSharedObject<RigidBody>(L, body);
    return 1;
}
// 0x08141ee0
static int RigidBody_setMaterial(lua_State* L)
{
    RigidBody* body = luax::checkObject<RigidBody>(L, 1);
    PhysicsMaterial* material = luax::checkObject<PhysicsMaterial>(L, 2);
    body->setMaterial(material);
    return 0;
}
// 0x08142560
static int RigidBody_setMass(lua_State* L)
{
    RigidBody* body = luax::checkObject<RigidBody>(L, 1);
    body->setMass((float)luaL_checknumber(L, 2));
    return 0;
}
// 0x081423e0
static int RigidBody_setCenterOfMassOffset(lua_State* L)
{
    RigidBody* body = luax::checkObject<RigidBody>(L, 1);
    body->setCenterOfMassOffset(luax::checkVector3_alt(L, 2));
    return 0;
}
// 0x0814d550: setBodyFlag(flag, on)
static int RigidBody_setBodyFlag(lua_State* L)
{
    RigidBody* body = luax::checkObject<RigidBody>(L, 1);
    // 0x08241aa0
    luax::Enum flags[] = {{"DisableCollision", 1}, {"DisableGravity", 2}, {"FreezePosX", 4},
                          {"FreezePosY", 8},       {"FreezePosZ", 16},    {"FreezeRotX", 32},
                          {"FreezeRotY", 64},      {"FreezeRotZ", 128},   {0, 0}};
    int flag = luax::checkEnum(L, 2, flags);
    bool enabled = luax::checkBool(L, 3);
    body->setBodyFlag((BodyFlag)flag, enabled);
    return 0;
}
// 0x08142350
static int RigidBody_setPosition(lua_State* L)
{
    RigidBody* body = luax::checkObject<RigidBody>(L, 1);
    body->setPosition(luax::checkVector3_alt(L, 2));
    return 0;
}
// 0x081422c0
static int RigidBody_setLinearVelocity(lua_State* L)
{
    RigidBody* body = luax::checkObject<RigidBody>(L, 1);
    body->setLinearVelocity(luax::checkVector3_alt(L, 2));
    return 0;
}
// 0x08142230
static int RigidBody_setAngularVelocity(lua_State* L)
{
    RigidBody* body = luax::checkObject<RigidBody>(L, 1);
    body->setAngularVelocity(luax::checkVector3_alt(L, 2));
    return 0;
}
// 0x081424e0
static int RigidBody_setLinearDamping(lua_State* L)
{
    RigidBody* body = luax::checkObject<RigidBody>(L, 1);
    body->setLinearDamping((float)luaL_checknumber(L, 2));
    return 0;
}
// 0x08142460
static int RigidBody_setAngularDamping(lua_State* L)
{
    RigidBody* body = luax::checkObject<RigidBody>(L, 1);
    body->setAngularDamping((float)luaL_checknumber(L, 2));
    return 0;
}
// 0x08143b10
static int RigidBody_getMaterial(lua_State* L)
{
    RigidBody* body = luax::checkObject<RigidBody>(L, 1);
    pushSharedObject<PhysicsMaterial>(L, body->getMaterial());
    return 1;
}
// 0x08141e70
static int RigidBody_getMass(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<RigidBody>(L, 1)->getMass());
    return 1;
}
// 0x081420a0
static int RigidBody_getPosition(lua_State* L)
{
    luax::pushVector(L, luax::checkObject<RigidBody>(L, 1)->getPosition());
    return 1;
}
// 0x08142010
static int RigidBody_getLinearVelocity(lua_State* L)
{
    luax::pushVector(L, luax::checkObject<RigidBody>(L, 1)->getLinearVelocity());
    return 1;
}
// 0x08141f80
static int RigidBody_getAngularVelocity(lua_State* L)
{
    luax::pushVector(L, luax::checkObject<RigidBody>(L, 1)->getAngularVelocity());
    return 1;
}
// 0x08141e00
static int RigidBody_getLinearDamping(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<RigidBody>(L, 1)->getLinearDamping());
    return 1;
}
// 0x08141d90
static int RigidBody_getAngularDamping(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<RigidBody>(L, 1)->getAngularDamping());
    return 1;
}
// 0x081421b0
static int RigidBody_addForce(lua_State* L)
{
    RigidBody* body = luax::checkObject<RigidBody>(L, 1);
    body->addForce(luax::checkVector3_alt(L, 2));
    return 0;
}
// 0x08142130
static int RigidBody_addVelocity(lua_State* L)
{
    RigidBody* body = luax::checkObject<RigidBody>(L, 1);
    body->addVelocity(luax::checkVector3_alt(L, 2));
    return 0;
}

const luaL_Reg RigidBody_methods[] = {{"create", RigidBody_create},
                                      {"setMaterial", RigidBody_setMaterial},
                                      {"setMass", RigidBody_setMass},
                                      {"setCenterOfMassOffset", RigidBody_setCenterOfMassOffset},
                                      {"setBodyFlag", RigidBody_setBodyFlag},
                                      {"setPosition", RigidBody_setPosition},
                                      {"setLinearVelocity", RigidBody_setLinearVelocity},
                                      {"setAngularVelocity", RigidBody_setAngularVelocity},
                                      {"setLinearDamping", RigidBody_setLinearDamping},
                                      {"setAngularDamping", RigidBody_setAngularDamping},
                                      {"getMaterial", RigidBody_getMaterial},
                                      {"getMass", RigidBody_getMass},
                                      {"getPosition", RigidBody_getPosition},
                                      {"getLinearVelocity", RigidBody_getLinearVelocity},
                                      {"getAngularVelocity", RigidBody_getAngularVelocity},
                                      {"getLinearDamping", RigidBody_getLinearDamping},
                                      {"getAngularDamping", RigidBody_getAngularDamping},
                                      {"addForce", RigidBody_addForce},
                                      {"addVelocity", RigidBody_addVelocity},
                                      {0, 0}};
const char* RigidBody_properties[] = {
    "Material",        "Mass",          "Position",       "LinearVelocity",
    "AngularVelocity", "LinearDamping", "AngularDamping", 0};

// ---- PhysicsMaterial -------------------------------------------------------------

// 0x08145e70: PhysicsMaterial.create(world)
static int PhysicsMaterial_create(lua_State* L)
{
    DynamicsWorld* world = luax::checkObject<DynamicsWorld>(L, 1);
    luax::createSharedObject<PhysicsMaterial>(L, world->createMaterial());
    return 1;
}
// 0x0813e910
static int PhysicsMaterial_setRestitution(lua_State* L)
{
    PhysicsMaterial* material = luax::checkObject<PhysicsMaterial>(L, 1);
    material->setRestitution((float)luaL_checknumber(L, 2));
    return 0;
}
// 0x0813e890
static int PhysicsMaterial_setStaticFriction(lua_State* L)
{
    PhysicsMaterial* material = luax::checkObject<PhysicsMaterial>(L, 1);
    material->setStaticFriction((float)luaL_checknumber(L, 2));
    return 0;
}
// 0x0813eae0
static int PhysicsMaterial_setDynamicFriction(lua_State* L)
{
    PhysicsMaterial* material = luax::checkObject<PhysicsMaterial>(L, 1);
    material->setDynamicFriction((float)luaL_checknumber(L, 2));
    return 0;
}
// 0x0813ea70
static int PhysicsMaterial_getRestitution(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<PhysicsMaterial>(L, 1)->getRestitution());
    return 1;
}
// 0x0813ea00
static int PhysicsMaterial_getStaticFriction(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<PhysicsMaterial>(L, 1)->getStaticFriction());
    return 1;
}
// 0x0813e990
static int PhysicsMaterial_getDynamicFriction(lua_State* L)
{
    lua_pushnumber(L, luax::checkObject<PhysicsMaterial>(L, 1)->getDynamicFriction());
    return 1;
}

const luaL_Reg PhysicsMaterial_methods[] = {
    {"create", PhysicsMaterial_create},
    {"setRestitution", PhysicsMaterial_setRestitution},
    {"setStaticFriction", PhysicsMaterial_setStaticFriction},
    {"setDynamicFriction", PhysicsMaterial_setDynamicFriction},
    {"getRestitution", PhysicsMaterial_getRestitution},
    {"getStaticFriction", PhysicsMaterial_getStaticFriction},
    {"getDynamicFriction", PhysicsMaterial_getDynamicFriction},
    {0, 0}};
const char* PhysicsMaterial_properties[] = {"Restitution", "StaticFriction", "DynamicFriction", 0};

// ---- CharacterController ---------------------------------------------------------

// 0x08148af0: CharacterController.create(world, node, radius, height)
static int CharacterController_create(lua_State* L)
{
    DynamicsWorld* world = luax::checkObject<DynamicsWorld>(L, 1);
    Node* node = luax::checkObject<Node>(L, 2);
    float radius = (float)luaL_checknumber(L, 3);
    float height = (float)luaL_checknumber(L, 4);
    CharacterController* controller = world->createCharacterController(node, radius, height);
    luax::createSharedObject<CharacterController>(L, controller);
    return 1;
}
// 0x0813d3e0
static int CharacterController_setPosition(lua_State* L)
{
    CharacterController* controller = luax::checkObject<CharacterController>(L, 1);
    controller->setPosition(luax::checkVector3_alt(L, 2));
    return 0;
}
// 0x0813d710
static int CharacterController_getPosition(lua_State* L)
{
    luax::pushVector(L, luax::checkObject<CharacterController>(L, 1)->getPosition());
    return 1;
}
// 0x081508b0: move(delta [, hitReportTable]); the table receives CollisionSides/Up/Down
// and an array of {position=, normal=, node=} hits.
static int CharacterController_move(lua_State* L)
{
    CharacterController* controller = luax::checkObject<CharacterController>(L, 1);
    Vec3 delta = luax::checkVector3(L, 2);
    if (lua_type(L, 3) == LUA_TTABLE)
    {
        ControllerHitReport report;
        controller->move(delta, &report);
        lua_pushboolean(L, report.collisionSides);
        lua_setfield(L, 3, "CollisionSides");
        lua_pushboolean(L, report.collisionUp);
        lua_setfield(L, 3, "CollisionUp");
        lua_pushboolean(L, report.collisionDown);
        lua_setfield(L, 3, "CollisionDown");
        for (int i = 0; i < report.numHits; ++i)
        {
            const ControllerHitReport::Hit& hit = report.hits[i];
            lua_createtable(L, 0, 0);
            luax::pushVector(L, hit.position);
            lua_setfield(L, -2, "position");
            luax::pushVector(L, hit.normal);
            lua_setfield(L, -2, "normal");
            pushNode(L, hit.node);
            lua_setfield(L, -2, "node");
            lua_rawseti(L, 3, i + 1);
        }
    }
    else
    {
        controller->move(delta, 0);
    }
    return 0;
}
// 0x0813d380
static int CharacterController_draw(lua_State* L)
{
    luax::checkObject<CharacterController>(L, 1)->draw();
    return 0;
}

const luaL_Reg CharacterController_methods[] = {{"create", CharacterController_create},
                                                {"setPosition", CharacterController_setPosition},
                                                {"getPosition", CharacterController_getPosition},
                                                {"move", CharacterController_move},
                                                {"draw", CharacterController_draw},
                                                {0, 0}};
