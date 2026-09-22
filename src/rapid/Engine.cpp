// engine_mod (0x08138ee0): registers the engine classes with Lua in the original order.
// The bindings themselves live in the Engine_*.cpp files.
#include "EngineBindings.h"

static const luaL_Reg noMethods[] = {{0, 0}};

void engine_mod(lua_State* L)
{
#if GRIMROCK_GAME >= 2
    // registered with the frame classes in the original (module 0x00409450)
    luax::registerClass(L, "HeightmapBuilder", HeightmapBuilder_methods, 0);
#endif
    luax::registerClass(L, "EngineSystems", EngineSystems_methods, 0);
    luax::registerClass(L, "Renderer", Renderer_methods, Renderer_properties);
    luax::registerClass(L, "Scene", Scene_methods, 0);
    luax::registerClass(L, "Node", Node_methods, Node_properties);
    luax::registerSubclass(L, "Camera", "Node", Camera_methods, 0);
    luax::registerClass(L, "CameraControls", CameraControls_methods, 0);
    luax::registerClass(L, "Mesh", Mesh_methods, 0);
    luax::registerClass(L, "RenderableMesh", RenderableMesh_methods, 0);
    luax::registerClass(L, "RenderableTexture", RenderableTexture_methods, 0);
    luax::registerClass(L, "RenderableShader", RenderableShader_methods, 0);
    luax::registerClass(L, "RenderBuffer", RenderBuffer_methods, 0);
    luax::registerClass(L, "RenderWindow", RenderWindow_methods, 0);
#if GRIMROCK_GAME >= 2
    luax::registerClass(L, "VPXPlayer", VPXPlayer_methods, 0);
    luax::registerClass(L, "SSAOFilter", SSAOFilter_methods, 0);
    luax::registerClass(L, "FogFilter", FogFilter_methods, 0);
    luax::registerClass(L, "Tonemapper", Tonemapper_methods, 0);
#endif
    luax::registerClass(L, "RenderEntity", RenderEntity_methods, RenderEntity_properties);
    luax::registerSubclass(L, "MeshEntity", "RenderEntity", MeshEntity_methods, 0);
    luax::registerClass(L, "Skeleton", noMethods, 0);
    luax::registerSubclass(L, "LightEntity", "RenderEntity", LightEntity_methods,
                           LightEntity_properties);
#if GRIMROCK_GAME >= 2
    luax::registerSubclass(L, "OccluderEntity", "RenderEntity", OccluderEntity_methods, 0);
#endif
    luax::registerSubclass(L, "ParticleEntity", "RenderEntity", ParticleEntity_methods, 0);
    luax::registerClass(L, "ParticleSystem", ParticleSystem_methods, 0);
    luax::registerClass(L, "ParticleEmitter", ParticleEmitter_methods, ParticleEmitter_properties);
    luax::registerClass(L, "MeshCDF", MeshCDF_methods, 0);
    luax::registerClass(L, "Model", Model_methods, 0);
    luax::registerClass(L, "Material", Material_methods, Material_properties);
#if GRIMROCK_GAME < 2
    luax::registerClass(L, "MaterialLibrary", MaterialLibrary_methods, 0);
#endif
    luax::registerClass(L, "Font", Font_methods, 0);
    luax::registerClass(L, "Animation", Animation_methods, Animation_properties);
    luax::registerClass(L, "AnimationController", AnimationController_methods, 0);
    luax::registerClass(L, "AnimationState", AnimationState_methods, AnimationState_properties);
    luax::registerClass(L, "AssetProcessor", AssetProcessor_methods, 0);
    luax::registerClass(L, "DebugDraw", DebugDraw_methods, 0);
    luax::registerClass(L, "ImmediateMode", ImmediateMode_methods, 0);
    luax::registerClass(L, "Graphics", Graphics_methods, 0);
    luax::registerClass(L, "PhysicsEngine", PhysicsEngine_methods, 0);
    luax::registerClass(L, "DynamicsWorld", DynamicsWorld_methods, 0);
    luax::registerClass(L, "CollisionMesh", CollisionMesh_methods, 0);
    luax::registerClass(L, "CollisionShape", CollisionShape_methods, 0);
    luax::registerSubclass(L, "PlaneShape", "CollisionShape", noMethods, 0);
    luax::registerSubclass(L, "BoxShape", "CollisionShape", noMethods, 0);
    luax::registerSubclass(L, "SphereShape", "CollisionShape", noMethods, 0);
    luax::registerSubclass(L, "MeshShape", "CollisionShape", noMethods, 0);
    luax::registerClass(L, "RigidBody", RigidBody_methods, RigidBody_properties);
    luax::registerClass(L, "PhysicsMaterial", PhysicsMaterial_methods, PhysicsMaterial_properties);
    luax::registerClass(L, "CharacterController", CharacterController_methods, 0);
    luax::registerClass(L, "AudioEngine", AudioEngine_methods, 0);
    luax::registerClass(L, "AudioWorld", AudioWorld_methods, 0);
    luax::registerSubclass(L, "AudioListener", "Node", AudioListener_methods, 0);
    luax::registerClass(L, "Sample", Sample_methods, Sample_properties);
    luax::registerClass(L, "SoundSource", SoundSource_methods, SoundSource_properties);
}
