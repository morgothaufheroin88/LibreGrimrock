// Reconstructed from Grimrock.bin.x86 Scene.cpp.
#include "engine/Scene.h"
#include "engine/SpatialDS.h"
#include <cstring>

namespace engine
{

// 0x080ef820
Scene::Scene() : m_pSpatialDS(0) {}
// 0x080efa90
Scene::~Scene()
{
    for (int i = 0; i < m_nodes.size(); ++i)
        delete m_nodes[i];
    m_nodes.clear();
    delete m_pSpatialDS;
}
// 0x080efbf0
Scene* Scene::createSimpleScene()
{
    Scene* scene = new Scene;
    scene->m_pSpatialDS = new SimpleSpatialDS;
    return scene;
}
// 0x080efb60
Scene* Scene::createQuadTreeScene(float size, int levels)
{
    Scene* scene = new Scene;
    scene->m_pSpatialDS = new QuadTreeSpatialDS(size, levels);
    return scene;
}
// 0x080ef9b0
Node* Scene::addNode()
{
    Node* node = new Node;
    node->m_pScene = this;
    m_nodes.push_back(node);
    return node;
}
// 0x080f0130: recursive removal deletes the subtree bottom-up first.
void Scene::removeNode(Node& node, bool recursive)
{
    if (recursive)
    {
        Node* child = node.getFirstChild();
        while (child)
        {
            Node* next = child->getNextSibling();
            removeNode(*child, true);
            child = next;
        }
    }
    m_nodes.remove(&node);
    delete &node;
}
// 0x080ef940
Node* Scene::findNode(const char* name)
{
    for (int i = 0; i < m_nodes.size(); ++i)
        if (strcmp(m_nodes[i]->getName().c_str(), name) == 0)
            return m_nodes[i];
    return 0;
}
// 0x080ef850
void Scene::query(NodeVisitor& visitor)
{
    for (int i = 0; i < m_nodes.size(); ++i)
        visitor.visit(m_nodes[i]);
}
// 0x080ef890
void Scene::query(const core::AABox3& box, NodeVisitor& visitor)
{
    m_pSpatialDS->query(box, visitor);
}
// 0x080ef8b0
void Scene::query(const core::Sphere3& sphere, NodeVisitor& visitor)
{
    m_pSpatialDS->query(sphere, visitor);
}
// 0x080ef8d0
void Scene::query(const core::Ray3& ray, NodeVisitor& visitor)
{
    m_pSpatialDS->query(ray, visitor);
}
// 0x080ef8f0
void Scene::query(const core::Plane* planes, int numPlanes, NodeVisitor& visitor)
{
    m_pSpatialDS->query(planes, numPlanes, visitor);
}

} // namespace engine
