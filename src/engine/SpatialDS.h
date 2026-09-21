// Spatial acceleration structures, from SpatialDS.cpp (0x0810dab0-0x0810fb80).
#pragma once
#include "core/Array.h"
#include "core/Prim.h"
#include "engine/Node.h"

namespace engine
{

class RenderEntity;

class SpatialNode
{
  public:
    explicit SpatialNode(RenderEntity* entity) : m_pEntity(entity) {}
    virtual ~SpatialNode() {}
    virtual void notifyBoundChanged() = 0;
    RenderEntity* getEntity() const
    {
        return m_pEntity;
    }

  protected:
    RenderEntity* m_pEntity;
};

class SpatialDS
{
  public:
    virtual ~SpatialDS() {}
    virtual SpatialNode* addEntity(RenderEntity& entity) = 0;
    virtual void removeEntity(SpatialNode& node) = 0;
    virtual void query(const core::AABox3& box, NodeVisitor& visitor) = 0;
    virtual void query(const core::Sphere3& sphere, NodeVisitor& visitor) = 0;
    virtual void query(const core::Ray3& ray, NodeVisitor& visitor) = 0;
    virtual void query(const core::Plane* planes, int numPlanes, NodeVisitor& visitor) = 0;
};

// Linear list; every query tests every entity.
class SimpleSpatialDS : public SpatialDS
{
  public:
    class SimpleSpatialNode : public SpatialNode
    {
      public:
        explicit SimpleSpatialNode(RenderEntity* entity) : SpatialNode(entity) {}
        void notifyBoundChanged() {}
    };
    SimpleSpatialDS();
    ~SimpleSpatialDS();
    SpatialNode* addEntity(RenderEntity& entity);
    void removeEntity(SpatialNode& node);
    void query(const core::AABox3& box, NodeVisitor& visitor);
    void query(const core::Sphere3& sphere, NodeVisitor& visitor);
    void query(const core::Ray3& ray, NodeVisitor& visitor);
    void query(const core::Plane* planes, int numPlanes, NodeVisitor& visitor);

  private:
    core::Array<SimpleSpatialNode*> m_nodes;
};

// Loose quadtree over the xz plane: level 0 is one node of the full size, each level
// down quarters it. Entities go to the finest level whose node size still exceeds
// their extent, indexed by the Morton number of their centre. Moves are batched as
// "dirty" entities and processed at the start of the next query.
class QuadTreeSpatialDS : public SpatialDS
{
  public:
    class QTEntity : public SpatialNode
    {
      public:
        QTEntity(QuadTreeSpatialDS* ds, RenderEntity* entity);
        ~QTEntity() {}
        void notifyBoundChanged();

        QuadTreeSpatialDS* m_pDS;
        core::AABox3 m_bounds;
        bool m_dirty;
        bool m_unbounded;
        int m_level;
        int m_index;
        QTEntity* m_pNext;
        QTEntity* m_pPrev;
    };
    struct QTNode
    {
        core::AABox3 bounds;
        bool dirty;
        int numEnts; // entities in this node and all descendants
        QTEntity* pFirstEnt;
    };

    QuadTreeSpatialDS(float size, int levels);
    ~QuadTreeSpatialDS();
    SpatialNode* addEntity(RenderEntity& entity);
    void removeEntity(SpatialNode& node);
    void query(const core::AABox3& box, NodeVisitor& visitor);
    void query(const core::Sphere3& sphere, NodeVisitor& visitor);
    void query(const core::Ray3& ray, NodeVisitor& visitor);
    void query(const core::Plane* planes, int numPlanes, NodeVisitor& visitor);
    void drawNodes(int level);

  private:
    void addEntity(QTEntity* ent);
    void removeEntity(QTEntity* ent);
    void processDirtyEnts();
    void recomputeBounds(int level, int index);
    void visitUnboundedEnts(NodeVisitor& visitor);
    void query(const core::AABox3& box, NodeVisitor& visitor, int level, int index);
    void query(const core::Sphere3& sphere, NodeVisitor& visitor, int level, int index);
    void query(const core::Ray3& ray, NodeVisitor& visitor, int level, int index);
    void query(const core::Plane* planes, int numPlanes, NodeVisitor& visitor, int level,
               int index);
    static unsigned short MortonNumber(int x, int y);
    QTNode& node(int level, int index)
    {
        return m_ppLevels[level][index];
    }

    float m_size;
    int m_numLevels;
    QTNode* m_pNodes;
    QTNode** m_ppLevels;
    core::Array<QTEntity*> m_dirtyEnts;
    core::Array<QTEntity*> m_unboundedEnts;
};

} // namespace engine
