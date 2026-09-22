// Internal header of the engine Lua module (Engine.cpp in the original, split into
// Engine_*.cpp files here). Declares the class tables and helpers shared by them.
#pragma once
#include "engine/Animation.h"
#include "engine/AssetProcessor.h"
#include "engine/AudioEngine.h"
#include "engine/Camera.h"
#include "engine/EngineSystems.h"
#include "engine/Font.h"
#include "engine/Material.h"
#include "engine/Mesh.h"
#include "engine/Model.h"
#include "engine/Node.h"
#include "engine/ParticleSystem.h"
#include "engine/PhysicsEngine.h"
#include "engine/RenderEntity.h"
#include "engine/Renderer.h"
#include "engine/Scene.h"
#include "engine/Texture.h"
#include "luax.h"

// Class table names of the bound engine types.
LUAX_CLASS(engine::EngineSystems, "EngineSystems")
LUAX_CLASS(engine::Renderer, "Renderer")
LUAX_CLASS(engine::Scene, "Scene")
LUAX_CLASS(engine::Node, "Node")
LUAX_CLASS(engine::Camera, "Camera")
LUAX_CLASS(engine::CameraControls, "CameraControls")
LUAX_CLASS(engine::Mesh, "Mesh")
LUAX_CLASS(engine::RenderableMesh, "RenderableMesh")
LUAX_CLASS(engine::RenderableTexture, "RenderableTexture")
LUAX_CLASS(engine::RenderableShader, "RenderableShader")
LUAX_CLASS(engine::RenderWindow, "RenderWindow")
LUAX_CLASS(engine::RenderEntity, "RenderEntity")
LUAX_CLASS(engine::MeshEntity, "MeshEntity")
LUAX_CLASS(engine::LightEntity, "LightEntity")
LUAX_CLASS(engine::ParticleEntity, "ParticleEntity")
LUAX_CLASS(engine::ParticleSystem, "ParticleSystem")
LUAX_CLASS(engine::ParticleEmitter, "ParticleEmitter")
LUAX_CLASS(engine::MeshCDF, "MeshCDF")
LUAX_CLASS(engine::Model, "Model")
LUAX_CLASS(engine::Material, "Material")
#if GRIMROCK_GAME >= 2
LUAX_CLASS(engine::VPXPlayer, "VPXPlayer")
LUAX_CLASS(engine::SSAOFilter, "SSAOFilter")
LUAX_CLASS(engine::FogFilter, "FogFilter")
LUAX_CLASS(engine::Tonemapper, "Tonemapper")
LUAX_CLASS(engine::OccluderEntity, "OccluderEntity")
#else
LUAX_CLASS(engine::MaterialLibrary, "MaterialLibrary")
#endif
LUAX_CLASS(engine::Font, "Font")
LUAX_CLASS(engine::Animation, "Animation")
LUAX_CLASS(engine::AnimationController, "AnimationController")
LUAX_CLASS(engine::AnimationState, "AnimationState")
LUAX_CLASS(engine::AssetProcessor, "AssetProcessor")
LUAX_CLASS(engine::PhysicsEngine, "PhysicsEngine")
LUAX_CLASS(engine::DynamicsWorld, "DynamicsWorld")
LUAX_CLASS(engine::CollisionMesh, "CollisionMesh")
LUAX_CLASS(engine::CollisionShape, "CollisionShape")
LUAX_CLASS(engine::PlaneShape, "PlaneShape")
LUAX_CLASS(engine::BoxShape, "BoxShape")
LUAX_CLASS(engine::SphereShape, "SphereShape")
LUAX_CLASS(engine::MeshShape, "MeshShape")
LUAX_CLASS(engine::RigidBody, "RigidBody")
LUAX_CLASS(engine::PhysicsMaterial, "PhysicsMaterial")
LUAX_CLASS(engine::CharacterController, "CharacterController")
LUAX_CLASS(engine::AudioEngine, "AudioEngine")
LUAX_CLASS(engine::AudioWorld, "AudioWorld")
LUAX_CLASS(engine::AudioListener, "AudioListener")
LUAX_CLASS(engine::Sample, "Sample")
LUAX_CLASS(engine::SoundSource, "SoundSource")
LUAX_CLASS(engine::Skeleton, "Skeleton")

// The engine systems created by EngineSystems.create (0x082c...).
extern engine::EngineSystems* g_pEngineSystems;
void shutdownEngine();

// String tables of the enums accepted from Lua.
extern luax::Enum g_renderEngines[];
extern luax::Enum g_lightTypes[];
extern luax::Enum g_physicsEngines[];
extern luax::Enum g_audioEngines[];
extern luax::Enum g_blendModes[];
extern luax::Enum g_textureFilterModes[];
extern luax::Enum g_textureAddressModes[];
extern luax::Enum g_particleEmitterBlendModes[];
#if GRIMROCK_GAME >= 2
extern luax::Enum g_rendererFlags[];
extern luax::Enum g_fogModes[];
extern luax::Enum g_renderBufferFormats[];
extern luax::Enum g_particleEmitterShapes[];
#endif

// Pushes the proxy of an object, creating one with a shared reference when needed.
template <class T> void pushSharedObject(lua_State* L, T* object)
{
    if (!object)
    {
        lua_pushnil(L);
        return;
    }
    luax::pushObject(L, object);
    if (!lua_isnil(L, -1))
        return;
    lua_pop(L, 1);
    luax::createSharedObject<T>(L, object);
}
// 0x0813bc60: nodes are owned by their scene, the proxies never delete them.
void pushNode(lua_State* L, engine::Node* node);
// Disposes the proxies of a node and its subtree (0x08150c40).
void disposeNodes(lua_State* L, engine::Node* node);
// Pushes the render entity of a node as its concrete class.
void pushRenderEntity(lua_State* L, engine::RenderEntity* entity);

// Method and property tables of every class, in the files named after them.
extern const luaL_Reg EngineSystems_methods[];
extern const luaL_Reg Renderer_methods[];
extern const char* Renderer_properties[];
extern const luaL_Reg Scene_methods[];
extern const luaL_Reg Node_methods[];
extern const char* Node_properties[];
extern const luaL_Reg Camera_methods[];
extern const luaL_Reg CameraControls_methods[];
extern const luaL_Reg Mesh_methods[];
extern const luaL_Reg RenderableMesh_methods[];
extern const luaL_Reg RenderableTexture_methods[];
extern const luaL_Reg RenderableShader_methods[];
extern const luaL_Reg RenderBuffer_methods[];
extern const luaL_Reg RenderWindow_methods[];
extern const luaL_Reg RenderEntity_methods[];
extern const char* RenderEntity_properties[];
extern const luaL_Reg MeshEntity_methods[];
extern const luaL_Reg LightEntity_methods[];
extern const char* LightEntity_properties[];
extern const luaL_Reg ParticleEntity_methods[];
extern const luaL_Reg ParticleSystem_methods[];
extern const luaL_Reg ParticleEmitter_methods[];
extern const char* ParticleEmitter_properties[];
extern const luaL_Reg MeshCDF_methods[];
extern const luaL_Reg Model_methods[];
extern const luaL_Reg Material_methods[];
extern const char* Material_properties[];
#if GRIMROCK_GAME >= 2
extern const luaL_Reg HeightmapBuilder_methods[];
extern const luaL_Reg VPXPlayer_methods[];
extern const luaL_Reg SSAOFilter_methods[];
extern const luaL_Reg FogFilter_methods[];
extern const luaL_Reg Tonemapper_methods[];
extern const luaL_Reg OccluderEntity_methods[];
#else
extern const luaL_Reg MaterialLibrary_methods[];
#endif
extern const luaL_Reg Font_methods[];
extern const luaL_Reg Animation_methods[];
extern const char* Animation_properties[];
extern const luaL_Reg AnimationController_methods[];
extern const luaL_Reg AnimationState_methods[];
extern const char* AnimationState_properties[];
extern const luaL_Reg AssetProcessor_methods[];
extern const luaL_Reg DebugDraw_methods[];
extern const luaL_Reg ImmediateMode_methods[];
extern const luaL_Reg Graphics_methods[];
extern const luaL_Reg PhysicsEngine_methods[];
extern const luaL_Reg DynamicsWorld_methods[];
extern const luaL_Reg CollisionMesh_methods[];
extern const luaL_Reg CollisionShape_methods[];
extern const luaL_Reg RigidBody_methods[];
extern const char* RigidBody_properties[];
extern const luaL_Reg PhysicsMaterial_methods[];
extern const char* PhysicsMaterial_properties[];
extern const luaL_Reg CharacterController_methods[];
extern const luaL_Reg AudioEngine_methods[];
extern const luaL_Reg AudioWorld_methods[];
extern const luaL_Reg AudioListener_methods[];
extern const luaL_Reg Sample_methods[];
extern const char* Sample_properties[];
extern const luaL_Reg SoundSource_methods[];
extern const char* SoundSource_properties[];

void engine_mod(lua_State* L);
