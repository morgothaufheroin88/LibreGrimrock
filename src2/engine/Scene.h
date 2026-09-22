// Scene: flat node list plus a spatial data structure, from grimrock2.exe Scene.cpp
// (0x004aa3a0-0x004aa860).
#pragma once
#include "core/Array.h"
#include "core/Prim.h"
#include "engine/Node.h"

namespace engine
{

class SpatialDS;
class RenderEntity;

class Scene
{
  public:
    Scene();
    ~Scene();
    // 0x004aa630
    static Scene* createSimpleScene();
    // 0x004aa6d0
    static Scene* createQuadTreeScene(float size, int levels);

    // 0x004aa560
    Node* addNode();
    // 0x004aa780
    void removeNode(Node& node, bool recursive);
    // 0x004aa3d0
    Node* findNode(const char* name);
    // 0x004aa450-0x004aa480
    void query(const core::AABox3& box, core::Array<RenderEntity*>& out);
    void query(const core::Sphere3& sphere, core::Array<RenderEntity*>& out);
    void query(const core::Ray3& ray, core::Array<RenderEntity*>& out);
    void query(const core::Plane* planes, int numPlanes, core::Array<RenderEntity*>& out);
    SpatialDS* getSpatialDS() const
    {
        return m_pSpatialDS;
    }
    const core::Array<Node*>& getNodes() const
    {
        return m_nodes;
    }
    // 0x004aa5f0 / 0x004aa7f0: bookkeeping for Node::setScene
    void attachNode(Node& node);
    void detachNode(Node& node);

  private:
    core::Array<Node*> m_nodes;
    SpatialDS* m_pSpatialDS;
};

} // namespace engine
