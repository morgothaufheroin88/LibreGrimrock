// Physics interface and the null implementation shipped in the Linux build,
// reconstructed from PhysicsEngine.cpp (0x080d6df0-0x080d79b0).
#pragma once
#include "core/Array.h"
#include "core/Prim.h"
#include "core/SharedPtr.h"
#include "core/String.h"
#include "core/Vector.h"
#include "engine/Node.h"

namespace engine
{

class Mesh;

enum BodyFlag
{
    DisableCollision = 1,
    DisableGravity = 2,
    FreezePosX = 4,
    FreezePosY = 8,
    FreezePosZ = 16,
    FreezeRotX = 32,
    FreezeRotY = 64,
    FreezeRotZ = 128
};

// Collision meshes are cached by filename (0x080d6fd0).
class CollisionMesh
{
  public:
    CollisionMesh();
    virtual ~CollisionMesh();
    virtual void init(Mesh& mesh, bool convex) = 0;
    virtual void load(const char* filename) = 0;
    const core::String& getFilename() const
    {
        return m_filename;
    }
    static CollisionMesh* getCollisionMeshByFilename(const char* filename);
    static core::Array<CollisionMesh*> sm_meshes;

  protected:
    core::String m_filename;
};

// Shapes are plain structs tagged by type (see CollisionShape_create* in Engine.cpp).
struct CollisionShape
{
    enum Type
    {
        Plane = 0,
        Box = 1,
        Sphere = 2,
        Mesh = 3
    };
    int type;
    explicit CollisionShape(int t) : type(t) {}
    virtual ~CollisionShape() {}
};
struct PlaneShape : CollisionShape
{
    core::Vec3 normal;
    float d;
    PlaneShape(const core::Vec3& n, float d_) : CollisionShape(Plane), normal(n), d(d_) {}
};
struct BoxShape : CollisionShape
{
    core::Vec3 halfExtents;
    explicit BoxShape(const core::Vec3& e) : CollisionShape(Box), halfExtents(e) {}
};
struct SphereShape : CollisionShape
{
    float radius;
    explicit SphereShape(float r) : CollisionShape(Sphere), radius(r) {}
};
struct MeshShape : CollisionShape
{
    CollisionMesh* mesh;
    explicit MeshShape(CollisionMesh* m) : CollisionShape(Mesh), mesh(m) {}
};

struct BodyDesc
{
    enum Type
    {
        Static = 0,
        Dynamic = 1
    };
    int type;
    float mass;
    int group;
    BodyDesc() : type(Static), mass(1.0f), group(0) {}
};

struct ContactReport
{
    Node* node1;
    Node* node2;
    core::Vec3 sumNormalForce;
};

// Filled by CharacterController::move (0x388 bytes).
struct ControllerHitReport
{
    static constexpr int MaxHits = 32;
    struct Hit
    {
        Node* node;
        core::Vec3 position;
        core::Vec3 normal;
    };
    bool collisionSides;
    bool collisionUp;
    bool collisionDown;
    Hit hits[MaxHits];
    int numHits;
    ControllerHitReport()
        : collisionSides(false), collisionUp(false), collisionDown(false), numHits(0)
    {
    }
};

class PhysicsMaterial
{
  public:
    virtual ~PhysicsMaterial() {}
    virtual void setRestitution(float r) = 0;
    virtual void setStaticFriction(float f) = 0;
    virtual void setDynamicFriction(float f) = 0;
    virtual float getRestitution() = 0;
    virtual float getStaticFriction() = 0;
    virtual float getDynamicFriction() = 0;
};

class RigidBody : public Component
{
  public:
    RigidBody() : Component(RigidBodyComponent) {}
    virtual ~RigidBody() {}
    virtual void setMaterial(PhysicsMaterial* material) = 0;
    virtual void setMass(float mass) = 0;
    virtual void setCenterOfMassOffset(const core::Vec3& offset) = 0;
    virtual void setBodyFlag(BodyFlag flag, bool on) = 0;
    virtual void setPosition(const core::Vec3& p) = 0;
    virtual void setLinearVelocity(const core::Vec3& v) = 0;
    virtual void setAngularVelocity(const core::Vec3& v) = 0;
    virtual void setLinearDamping(float d) = 0;
    virtual void setAngularDamping(float d) = 0;
    virtual PhysicsMaterial* getMaterial() const = 0;
    virtual float getMass() const = 0;
    virtual core::Vec3 getPosition() const = 0;
    virtual core::Vec3 getLinearVelocity() const = 0;
    virtual core::Vec3 getAngularVelocity() const = 0;
    virtual float getLinearDamping() const = 0;
    virtual float getAngularDamping() const = 0;
    virtual void addForce(const core::Vec3& f) = 0;
    virtual void addVelocity(const core::Vec3& v) = 0;
};

class CharacterController
{
  public:
    virtual ~CharacterController() {}
    virtual void setPosition(const core::Vec3& p) = 0;
    virtual core::Vec3 getPosition() const = 0;
    virtual void move(const core::Vec3& delta, ControllerHitReport* report) = 0;
    virtual void draw() = 0;
};

class DynamicsWorld
{
  public:
    virtual ~DynamicsWorld() {}
    virtual RigidBody* createBody(const CollisionShape& shape, const BodyDesc& desc) = 0;
    virtual PhysicsMaterial* createMaterial() = 0;
    virtual CharacterController* createCharacterController(Node* node, float radius,
                                                           float height) = 0;
    virtual void setGravity(const core::Vec3& g) = 0;
    virtual void enableCollisions(int group1, int group2, bool on) = 0;
    virtual void enableContactReporting(int group1, int group2, bool on) = 0;
    virtual bool raycastAnyShape(const core::Ray3& ray, float maxDistance, int groups) = 0;
    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
    virtual const ContactReport* getContactReport() = 0;
};

class PhysicsEngine
{
  public:
    enum Engine
    {
        Engine_Null = 0,
        Engine_PhysX = 1
    };
    virtual ~PhysicsEngine() {}
    virtual DynamicsWorld* createWorld() = 0;
    virtual CollisionMesh* createCollisionMesh() = 0;
    // 0x080d70b0: only the null engine exists in this build.
    static PhysicsEngine* create(int engine);
    static PhysicsEngine* sm_pActivePhysicsEngine;
};

// ---- null implementation -----------------------------------------------------------

class CollisionMeshNull : public CollisionMesh
{
  public:
    void init(Mesh& mesh, bool convex) {}
    void load(const char* filename) {}
};

class PhysicsMaterialNull : public PhysicsMaterial
{
  public:
    void setRestitution(float r) {}
    void setStaticFriction(float f) {}
    void setDynamicFriction(float f) {}
    float getRestitution()
    {
        return 0.0f;
    }
    float getStaticFriction()
    {
        return 0.0f;
    }
    float getDynamicFriction()
    {
        return 0.0f;
    }
};

class RigidBodyNull : public RigidBody
{
  public:
    RigidBodyNull() : m_pMaterial(0), m_mass(1.0f) {}
    void setMaterial(PhysicsMaterial* material) {}
    void setMass(float mass) {}
    void setCenterOfMassOffset(const core::Vec3& offset) {}
    void setBodyFlag(BodyFlag flag, bool on) {}
    void setPosition(const core::Vec3& p) {}
    void setLinearVelocity(const core::Vec3& v) {}
    void setAngularVelocity(const core::Vec3& v) {}
    void setLinearDamping(float d) {}
    void setAngularDamping(float d) {}
    PhysicsMaterial* getMaterial() const
    {
        return m_pMaterial;
    }
    float getMass() const
    {
        return m_mass;
    }
    core::Vec3 getPosition() const
    {
        return core::Vec3(0, 0, 0);
    }
    core::Vec3 getLinearVelocity() const
    {
        return core::Vec3(0, 0, 0);
    }
    core::Vec3 getAngularVelocity() const
    {
        return core::Vec3(0, 0, 0);
    }
    float getLinearDamping() const
    {
        return 0.0f;
    }
    float getAngularDamping() const
    {
        return 0.0f;
    }
    void addForce(const core::Vec3& f) {}
    void addVelocity(const core::Vec3& v) {}

  private:
    PhysicsMaterial* m_pMaterial;
    float m_mass;
};

class CharacterControllerNull : public CharacterController
{
  public:
    void setPosition(const core::Vec3& p) {}
    core::Vec3 getPosition() const
    {
        return core::Vec3(0, 0, 0);
    }
    void move(const core::Vec3& delta, ControllerHitReport* report) {}
    void draw() {}
};

class DynamicsWorldNull : public DynamicsWorld
{
  public:
    RigidBody* createBody(const CollisionShape& shape, const BodyDesc& desc)
    {
        return new RigidBodyNull;
    }
    PhysicsMaterial* createMaterial()
    {
        return new PhysicsMaterialNull;
    }
    CharacterController* createCharacterController(Node* node, float radius, float height)
    {
        return new CharacterControllerNull;
    }
    void setGravity(const core::Vec3& g) {}
    void enableCollisions(int group1, int group2, bool on) {}
    void enableContactReporting(int group1, int group2, bool on) {}
    bool raycastAnyShape(const core::Ray3& ray, float maxDistance, int groups)
    {
        return false;
    }
    void beginFrame() {}
    void endFrame() {}
    const ContactReport* getContactReport()
    {
        return 0;
    }
};

class PhysicsEngineNull : public PhysicsEngine
{
  public:
    DynamicsWorld* createWorld()
    {
        return new DynamicsWorldNull;
    }
    CollisionMesh* createCollisionMesh()
    {
        return new CollisionMeshNull;
    }
};

// 0x080d6df0 / 0x080d7130
CollisionMesh* createCollisionMesh(Mesh& mesh, bool convex);
CollisionMesh* loadCollisionMesh(const char* filename);

} // namespace engine
