// Reconstructed from Grimrock.bin.x86 PhysicsEngine.cpp.
#include "engine/PhysicsEngine.h"
#include "core/Exception.h"
#include "core/Sys.h"
#include <cstring>

namespace engine
{

using namespace core;

Array<CollisionMesh*> CollisionMesh::sm_meshes;
PhysicsEngine* PhysicsEngine::sm_pActivePhysicsEngine = 0;

// 0x080d6fd0
CollisionMesh::CollisionMesh()
{
    sm_meshes.push_back(this);
}
// 0x080d6f20
CollisionMesh::~CollisionMesh()
{
    sm_meshes.remove(this);
}
// 0x080d6eb0
CollisionMesh* CollisionMesh::getCollisionMeshByFilename(const char* filename)
{
    for (int i = 0; i < sm_meshes.size(); ++i)
        if (strcmp(sm_meshes[i]->m_filename.c_str(), filename) == 0)
            return sm_meshes[i];
    return 0;
}

// 0x080d70b0
PhysicsEngine* PhysicsEngine::create(int engine)
{
    if (engine == Engine_Null)
    {
        sm_pActivePhysicsEngine = new PhysicsEngineNull;
        return sm_pActivePhysicsEngine;
    }
    throw Exception("Invalid physics engine");
}

// 0x080d6df0
CollisionMesh* createCollisionMesh(Mesh& mesh, bool convex)
{
    CollisionMesh* collisionMesh = PhysicsEngine::sm_pActivePhysicsEngine->createCollisionMesh();
    collisionMesh->init(mesh, convex);
    return collisionMesh;
}
// 0x080d7130
CollisionMesh* loadCollisionMesh(const char* filename)
{
    CollisionMesh* cached = CollisionMesh::getCollisionMeshByFilename(filename);
    if (cached)
        return cached;
    debugPrint("Load collision mesh %s\n", filename);
    CollisionMesh* collisionMesh = PhysicsEngine::sm_pActivePhysicsEngine->createCollisionMesh();
    collisionMesh->load(filename);
    return collisionMesh;
}

// 0x080da920
void Node::setRigidBody(RigidBody* body)
{
    setComponent(Component::RigidBodyComponent, body);
}

} // namespace engine
