// Scene graph node and component base of Legend of Grimrock 2, reconstructed from
// grimrock2.exe Node.cpp (0x004b6c00-0x004b7660). Node layout (0xd4 bytes): name,
// parent/next/prev/firstChild/lastChild links, local matrix, cached local-to-world and
// world-to-local matrices, three component slots and the owning scene; the spatial
// bookkeeping moved into the render entity.
#pragma once
#include "core/Matrix.h"
#include "core/SharedPtr.h"
#include "core/String.h"

namespace engine
{

class Node;
class Scene;
class MeshEntity;
class LightEntity;
class OccluderEntity;

class Component
{
  public:
    enum ComponentType
    {
        RenderEntityComponent = 0,
        RigidBodyComponent = 1,
        SoundSourceComponent = 2
    };

    explicit Component(ComponentType type) : m_pNode(0), m_type(type) {}
    virtual ~Component() {}
    // 0x004a72b0: no-ops in the base.
    virtual void addedToNode(Node* node) {}
    virtual void removedFromNode() {}
    Node* getNode() const
    {
        return m_pNode;
    }
    ComponentType getComponentType() const
    {
        return m_type;
    }

  protected:
    friend class Node;
    Node* m_pNode;
    ComponentType m_type;
};

class NodeVisitor
{
  public:
    virtual ~NodeVisitor() {}
    virtual void visit(Node* node) = 0;
};

class Node
{
  public:
    Node();
    Node(const Node& other);
    ~Node();
    // The only virtual: called whenever the world transform of this node changes
    // (the camera invalidates its planes).
    virtual void notifyMoved();

    const core::String& getName() const
    {
        return m_name;
    }
    void setName(const char* name)
    {
        m_name = name;
    }
    Node* getParent() const
    {
        return m_pParent;
    }
    Node* getNextSibling() const
    {
        return m_pNext;
    }
    Node* getPrevSibling() const
    {
        return m_pPrev;
    }
    Node* getFirstChild() const
    {
        return m_pFirstChild;
    }
    Node* getLastChild() const
    {
        return m_pLastChild;
    }
    Scene* getScene() const
    {
        return m_pScene;
    }
    // 0x004b6c20: moves the subtree to another scene, re-registering the components.
    void setScene(Scene* scene);

    void addTo(Node* parent);
    void remove();
    Node* findNode(const char* name);

    const core::Matrix4x3& getLocalMatrix() const
    {
        return m_localMatrix;
    }
    void setLocalMatrix(const core::Matrix4x3& m)
    {
        m_localMatrix = m;
        notifyMovedRecursively();
    }
    const core::Vec3& getPosition() const
    {
        return m_localMatrix.pos;
    }
    void setPosition(const core::Vec3& p)
    {
        m_localMatrix.pos = p;
        notifyMovedRecursively();
    }
    const core::Matrix3x3& getRotation() const
    {
        return m_localMatrix;
    }
    void setRotation(const core::Matrix3x3& r)
    {
        m_localMatrix.rotation() = r;
        notifyMovedRecursively();
    }
    const core::Matrix4x3& getLocalToWorldMatrix() const;
    const core::Matrix4x3& getWorldToLocalMatrix() const;
    core::Vec3 getWorldPosition() const
    {
        return getLocalToWorldMatrix().pos;
    }

    void move(const core::Vec3& delta);
    void rotate(const core::Matrix3x3& r);
    void rotateAbout(int axis, float angle);
    void transform(const core::Matrix4x3& m);
    void lookAt(const core::Vec3& target);

    void setComponent(Component::ComponentType type, Component* component);
    void addComponent(Component& component);
    Component* getComponent(Component::ComponentType type) const
    {
        return m_components[type].get();
    }
    void setRenderEntity(class RenderEntity* entity);
    void setRigidBody(class RigidBody* body);
    void setSoundSource(class SoundSource* source);
    class RenderEntity* getRenderEntity() const
    {
        return (class RenderEntity*)m_components[Component::RenderEntityComponent].get();
    }
    class RigidBody* getRigidBody() const
    {
        return (class RigidBody*)m_components[Component::RigidBodyComponent].get();
    }
    class SoundSource* getSoundSource() const
    {
        return (class SoundSource*)m_components[Component::SoundSourceComponent].get();
    }
    // 0x004b6d10 / 0x004b6d30 / 0x004b6d50: the render entity when it is of that type
    MeshEntity* getMeshEntity() const;
    LightEntity* getLightEntity() const;
    OccluderEntity* getOccluderEntity() const;
    void notifyMovedRecursively();

  private:
    friend class Scene;
    void validateMatrices() const;

    core::String m_name;
    Node* m_pParent;
    Node* m_pNext;
    Node* m_pPrev;
    Node* m_pFirstChild;
    Node* m_pLastChild;
    core::Matrix4x3 m_localMatrix;
    mutable core::Matrix4x3 m_localToWorld;
    mutable core::Matrix4x3 m_worldToLocal;
    mutable bool m_matricesValid;
    core::SharedPtr<Component> m_components[3];
    Scene* m_pScene;
};

} // namespace engine
