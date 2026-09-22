// Reconstructed from grimrock2.exe Scene.cpp.
#include "engine/Scene.h"
#include "engine/SpatialDS.h"
#include <cstring>

namespace engine
{

Scene::Scene() : m_pSpatialDS(0) {}
// 0x004aa4e0
Scene::~Scene()
{
    for (int i = 0; i < m_nodes.size(); ++i)
        delete m_nodes[i];
    m_nodes.clear();
    delete m_pSpatialDS;
}
// 0x004aa630
Scene* Scene::createSimpleScene()
{
    Scene* scene = new Scene;
    scene->m_pSpatialDS = new SimpleSpatialDS;
    return scene;
}
// 0x004aa6d0
Scene* Scene::createQuadTreeScene(float size, int levels)
{
    Scene* scene = new Scene;
    scene->m_pSpatialDS = new QuadTreeSpatialDS(size, levels);
    return scene;
}
// 0x004aa560
Node* Scene::addNode()
{
    Node* node = new Node;
    node->m_pScene = this;
    m_nodes.push_back(node);
    return node;
}
// 0x004aa780: recursive removal deletes the subtree bottom-up first.
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
// 0x004aa5f0
void Scene::attachNode(Node& node)
{
    m_nodes.push_back(&node);
    node.m_pScene = this;
}
// 0x004aa7f0
void Scene::detachNode(Node& node)
{
    m_nodes.remove(&node);
    node.m_pScene = 0;
}
// 0x004aa3d0
Node* Scene::findNode(const char* name)
{
    for (int i = 0; i < m_nodes.size(); ++i)
        if (strcmp(m_nodes[i]->getName().c_str(), name) == 0)
            return m_nodes[i];
    return 0;
}
// 0x004aa450 / 0x004aa460 / 0x004aa470 / 0x004aa480
void Scene::query(const core::AABox3& box, core::Array<RenderEntity*>& out)
{
    m_pSpatialDS->query(box, out);
}
void Scene::query(const core::Sphere3& sphere, core::Array<RenderEntity*>& out)
{
    m_pSpatialDS->query(sphere, out);
}
void Scene::query(const core::Ray3& ray, core::Array<RenderEntity*>& out)
{
    m_pSpatialDS->query(ray, out);
}
void Scene::query(const core::Plane* planes, int numPlanes, core::Array<RenderEntity*>& out)
{
    m_pSpatialDS->query(planes, numPlanes, out);
}

} // namespace engine
