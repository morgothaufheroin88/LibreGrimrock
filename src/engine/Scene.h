// Scene: flat node list plus a spatial data structure, from Scene.cpp (0x080ef820-0x080f0130).
#pragma once
#include "core/Array.h"
#include "core/Prim.h"
#include "engine/Node.h"

namespace engine
{

class SpatialDS;

class Scene
{
  public:
    Scene();
    ~Scene();
    static Scene* createSimpleScene();
    static Scene* createQuadTreeScene(float size, int levels);

    Node* addNode();
    void removeNode(Node& node, bool recursive);
    Node* findNode(const char* name);
    void query(NodeVisitor& visitor);
    void query(const core::AABox3& box, NodeVisitor& visitor);
    void query(const core::Sphere3& sphere, NodeVisitor& visitor);
    void query(const core::Ray3& ray, NodeVisitor& visitor);
    void query(const core::Plane* planes, int numPlanes, NodeVisitor& visitor);
    SpatialDS* getSpatialDS() const
    {
        return m_pSpatialDS;
    }
    const core::Array<Node*>& getNodes() const
    {
        return m_nodes;
    }

  private:
    core::Array<Node*> m_nodes;
    SpatialDS* m_pSpatialDS;
};

} // namespace engine
