// Spatial acceleration structures of Legend of Grimrock 2, from grimrock2.exe
// SpatialDS.cpp (0x004beed0-0x004c0770). Unlike the first game the structures hold the
// render entities directly (the entity remembers its slot) and the queries fill arrays.
#pragma once
#include "core/Array.h"
#include "core/Prim.h"

namespace engine
{

class RenderEntity;

class SpatialDS
{
  public:
    virtual ~SpatialDS() {}
    virtual void addEntity(RenderEntity& entity) = 0;
    virtual void removeEntity(RenderEntity& entity) = 0;
    // 0x004a7320: no-op unless the structure caches bounds
    virtual void notifyBoundChanged(RenderEntity& entity) {}
    virtual void query(const core::Plane* planes, int numPlanes,
                       core::Array<RenderEntity*>& out) = 0;
    virtual void query(const core::Ray3& ray, core::Array<RenderEntity*>& out) = 0;
    virtual void query(const core::Sphere3& sphere, core::Array<RenderEntity*>& out) = 0;
    virtual void query(const core::AABox3& box, core::Array<RenderEntity*>& out) = 0;
};

// Linear list; every query tests every entity (0x004bf5f0-0x004c0090).
class SimpleSpatialDS : public SpatialDS
{
  public:
    SimpleSpatialDS();
    ~SimpleSpatialDS();
    void addEntity(RenderEntity& entity);
    void removeEntity(RenderEntity& entity);
    void query(const core::Plane* planes, int numPlanes, core::Array<RenderEntity*>& out);
    void query(const core::Ray3& ray, core::Array<RenderEntity*>& out);
    void query(const core::Sphere3& sphere, core::Array<RenderEntity*>& out);
    void query(const core::AABox3& box, core::Array<RenderEntity*>& out);

  private:
    core::Array<RenderEntity*> m_entities;
};

// Loose quadtree over the xz plane: level 0 is one node of the full size, each level down
// quarters it. Entities go to the finest level whose node size still exceeds their extent,
// indexed by the Morton number of their centre; each node keeps its entities with their
// bounds and the bounds of its subtree. Moved entities are re-inserted at the start of the
// next query (0x004c00d0-0x004c0770).
class QuadTreeSpatialDS : public SpatialDS
{
  public:
    static constexpr unsigned short NoNode = 0xffff;
    // RenderEntity::m_spatialIndex values
    static constexpr int NotInTree = -1;
    static constexpr int Unbounded = -2;

    struct Entry
    {
        RenderEntity* pEntity;
        core::AABox3 bounds;
    };
    // 0x30 bytes in the original
    struct QTNode
    {
        int numEnts; // entities in this node and all descendants
        core::AABox3 bounds;
        core::Array<Entry> entries;
        unsigned short parent;
        unsigned short firstChild;
        bool dirty;
    };

    QuadTreeSpatialDS(float size, int levels);
    ~QuadTreeSpatialDS();
    void addEntity(RenderEntity& entity);
    void removeEntity(RenderEntity& entity);
    // 0x004bf9d0: removed now, queued for re-insertion
    void notifyBoundChanged(RenderEntity& entity);
    void query(const core::Plane* planes, int numPlanes, core::Array<RenderEntity*>& out);
    void query(const core::Ray3& ray, core::Array<RenderEntity*>& out);
    void query(const core::Sphere3& sphere, core::Array<RenderEntity*>& out);
    void query(const core::AABox3& box, core::Array<RenderEntity*>& out);

  private:
    // 0x004bfa10: re-inserts the moved entities and recomputes dirty subtree bounds.
    void prepare();
    // 0x004bf330
    void recomputeBounds(int nodeIndex);
    // 0x004bfff0
    void addUnboundedEntities(core::Array<RenderEntity*>& out);
    void query(const core::AABox3& box, core::Array<RenderEntity*>& out, int nodeIndex);
    void query(const core::Sphere3& sphere, core::Array<RenderEntity*>& out, int nodeIndex);
    void query(const core::Ray3& ray, core::Array<RenderEntity*>& out, int nodeIndex);
    void query(const core::Plane* planes, int numPlanes, core::Array<RenderEntity*>& out,
               int nodeIndex);
    // 0x004bfef0: six planes with the fast box test
    void queryFrustum(const core::Plane* planes, core::Array<RenderEntity*>& out, int nodeIndex);
    // 0x004bef10: interleave the bits of x (even) and y (odd)
    static unsigned int MortonNumber(int x, int y);

    float m_size;
    int m_numLevels;
    QTNode* m_pNodes;
    core::Array<unsigned short> m_levelStart; // first node index of each level
    core::Array<RenderEntity*> m_unboundedEnts;
    core::Array<RenderEntity*> m_dirtyEnts;
};

} // namespace engine
