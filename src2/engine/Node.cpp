// Reconstructed from grimrock2.exe Node.cpp.
#include "engine/Node.h"
#include "engine/RenderEntity.h"
#include "engine/Scene.h"
#include "engine/SpatialDS.h"
#include <cstring>

namespace engine
{

using namespace core;

// 0x004b7170
Node::Node()
    : m_pParent(0), m_pNext(0), m_pPrev(0), m_pFirstChild(0), m_pLastChild(0),
      m_matricesValid(true), m_pScene(0)
{
    m_localMatrix.makeIdentity();
    m_localToWorld.makeIdentity();
    m_worldToLocal.makeIdentity();
}

// Copies name and local transform only; links and components stay empty.
Node::Node(const Node& other)
    : m_name(other.m_name), m_pParent(0), m_pNext(0), m_pPrev(0), m_pFirstChild(0), m_pLastChild(0),
      m_localMatrix(other.m_localMatrix), m_matricesValid(false), m_pScene(0)
{
    m_localToWorld.makeIdentity();
    m_worldToLocal.makeIdentity();
}

// 0x004b73d0: unlinks from the parent, detaches the children and drops the components.
Node::~Node()
{
    remove();
    Node* child = m_pFirstChild;
    while (child)
    {
        Node* next = child->m_pNext;
        child->remove();
        child = next;
    }
    for (int i = 0; i < 3; ++i)
        setComponent((Component::ComponentType)i, 0);
}

void Node::notifyMoved() {}

// 0x004b70f0: appended as the last child of the new parent.
void Node::addTo(Node* parent)
{
    if (m_pParent == parent)
        return;
    if (m_pParent)
        remove();
    m_pParent = parent;
    if (parent->m_pFirstChild)
    {
        parent->m_pLastChild->m_pNext = this;
        m_pPrev = parent->m_pLastChild;
        m_pNext = 0;
        parent->m_pLastChild = this;
    }
    else
    {
        m_pNext = 0;
        m_pPrev = 0;
        parent->m_pFirstChild = this;
        parent->m_pLastChild = this;
    }
    notifyMovedRecursively();
}

// 0x004b6dc0
void Node::remove()
{
    if (!m_pParent)
        return;
    if (m_pPrev)
        m_pPrev->m_pNext = m_pNext;
    if (m_pNext)
        m_pNext->m_pPrev = m_pPrev;
    if (m_pParent->m_pFirstChild == this)
        m_pParent->m_pFirstChild = m_pNext;
    if (m_pParent->m_pLastChild == this)
        m_pParent->m_pLastChild = m_pPrev;
    m_pParent = 0;
    m_pNext = 0;
    m_pPrev = 0;
    notifyMovedRecursively();
}

// 0x004b6cc0: depth first search by name, this node included.
Node* Node::findNode(const char* name)
{
    if (strcmp(m_name.c_str(), name) == 0)
        return this;
    for (Node* child = m_pFirstChild; child; child = child->m_pNext)
    {
        Node* found = child->findNode(name);
        if (found)
            return found;
    }
    return 0;
}

// 0x004b7060: localToWorld = parent.localToWorld * local; worldToLocal = inverse.
void Node::validateMatrices() const
{
    m_localToWorld = m_localMatrix;
    if (m_pParent)
    {
        if (!m_pParent->m_matricesValid)
            m_pParent->validateMatrices();
        m_localToWorld = m_pParent->m_localToWorld * m_localMatrix;
    }
    m_worldToLocal = m_localToWorld;
    m_worldToLocal.invert();
    m_matricesValid = true;
}
// 0x004b6c00
const Matrix4x3& Node::getLocalToWorldMatrix() const
{
    if (!m_matricesValid)
        validateMatrices();
    return m_localToWorld;
}
// 0x004b7150
const Matrix4x3& Node::getWorldToLocalMatrix() const
{
    if (!m_matricesValid)
        validateMatrices();
    return m_worldToLocal;
}

// 0x004b6d70: invalidates the cached matrices of the whole subtree and informs the
// scene's spatial data structure about the moved render entities.
void Node::notifyMovedRecursively()
{
    m_matricesValid = false;
    notifyMoved();
    if (m_pScene && m_components[Component::RenderEntityComponent])
        m_pScene->getSpatialDS()->notifyBoundChanged(*getRenderEntity());
    for (Node* child = m_pFirstChild; child; child = child->m_pNext)
        child->notifyMovedRecursively();
}
// 0x004b6c20
void Node::setScene(Scene* scene)
{
    if (m_pScene == scene)
        return;
    for (int i = 0; i < 3; ++i)
        if (m_components[i])
            m_components[i]->removedFromNode();
    if (m_pScene)
        m_pScene->detachNode(*this);
    if (scene)
        scene->attachNode(*this);
    for (int i = 0; i < 3; ++i)
        if (m_components[i])
            m_components[i]->addedToNode(this);
    for (Node* child = m_pFirstChild; child; child = child->m_pNext)
        child->setScene(scene);
}
MeshEntity* Node::getMeshEntity() const
{
    RenderEntity* entity = getRenderEntity();
    return entity && entity->getEntityType() == RenderEntity::MeshEntityType ? (MeshEntity*)entity
                                                                             : 0;
}
LightEntity* Node::getLightEntity() const
{
    RenderEntity* entity = getRenderEntity();
    return entity && entity->getEntityType() == RenderEntity::LightEntityType ? (LightEntity*)entity
                                                                              : 0;
}
OccluderEntity* Node::getOccluderEntity() const
{
    RenderEntity* entity = getRenderEntity();
    return entity && entity->getEntityType() == RenderEntity::OccluderEntityType
               ? (OccluderEntity*)entity
               : 0;
}

// 0x004b6e90
void Node::move(const Vec3& delta)
{
    m_localMatrix.pos += delta;
    notifyMovedRecursively();
}
// 0x004b6f10: local rotation = r * local rotation
void Node::rotate(const Matrix3x3& r)
{
    m_localMatrix.rotation() = r * m_localMatrix.rotation();
    notifyMovedRecursively();
}
// 0x004b6f90
void Node::rotateAbout(int axis, float angle)
{
    Matrix3x3 r;
    if (axis == 0)
        r = Matrix3x3::createRotationX(angle);
    else if (axis == 1)
        r = Matrix3x3::createRotationY(angle);
    else if (axis == 2)
        r = Matrix3x3::createRotationZ(angle);
    else
        return;
    // Unlike rotate(), the axis is the node's own: local = local * r.
    m_localMatrix.rotation() = m_localMatrix.rotation() * r;
    notifyMovedRecursively();
}
// 0x004b6e10: local = m * local
void Node::transform(const Matrix4x3& m)
{
    m_localMatrix = m * m_localMatrix;
    notifyMovedRecursively();
}
// 0x004b7300: orient the local z axis towards a world space target.
void Node::lookAt(const Vec3& target)
{
    if (!m_matricesValid)
        validateMatrices();
    Vec3 dir = target - m_localToWorld.pos;
    m_localMatrix.rotation().lookAt(dir);
    if (m_pParent)
    {
        const Matrix4x3& parentInv = m_pParent->getWorldToLocalMatrix();
        m_localMatrix.rotation() = parentInv.rotation() * m_localMatrix.rotation();
    }
    notifyMovedRecursively();
}

// 0x004b7580 / 0x004b75e0 / 0x004b7640 (one instantiation per slot in the original)
void Node::setComponent(Component::ComponentType type, Component* component)
{
    SharedPtr<Component>& slot = m_components[type];
    if (slot)
    {
        slot->removedFromNode();
        slot->m_pNode = 0;
    }
    slot.reset(component);
    if (slot)
    {
        slot->m_pNode = this;
        slot->addedToNode(this);
    }
}
void Node::addComponent(Component& component)
{
    setComponent(component.getComponentType(), &component);
}
// 0x004b7580
void Node::setRenderEntity(RenderEntity* entity)
{
    setComponent(Component::RenderEntityComponent, entity);
}
// 0x004b75e0 / 0x004b7640 live with PhysicsEngine.cpp / AudioEngine.cpp where the
// component classes are complete.

} // namespace engine
