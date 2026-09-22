// Reconstructed from Grimrock.bin.x86 SpatialDS.cpp.
#include "engine/SpatialDS.h"
#include "core/DebugDraw.h"
#include "engine/RenderEntity.h"
#include <cfloat>
#include <cmath>
#include <cstring>

namespace engine
{

using namespace core;

// ---- SimpleSpatialDS -----------------------------------------------------------

// 0x0810dab0
SimpleSpatialDS::SimpleSpatialDS() {}
// 0x0810e510
SimpleSpatialDS::~SimpleSpatialDS()
{
    for (int i = 0; i < m_nodes.size(); ++i)
        delete m_nodes[i];
}
// 0x0810ee50
SpatialNode* SimpleSpatialDS::addEntity(RenderEntity& entity)
{
    SimpleSpatialNode* node = new SimpleSpatialNode(&entity);
    m_nodes.push_back(node);
    return node;
}
// 0x0810dae0
void SimpleSpatialDS::removeEntity(SpatialNode& node)
{
    m_nodes.remove((SimpleSpatialNode*)&node);
    delete &node;
}
// 0x0810edd0 / 0x0810ebf0 / 0x0810ea10 / 0x0810e820: unbounded entities are always
// visited, the rest are tested against their world bounds.
void SimpleSpatialDS::query(const AABox3& box, NodeVisitor& visitor)
{
    for (int i = 0; i < m_nodes.size(); ++i)
    {
        RenderEntity* entity = m_nodes[i]->getEntity();
        if (entity->isUnbounded() || testBoxBox(entity->getWorldBounds(), box))
            visitor.visit(entity->getNode());
    }
}
void SimpleSpatialDS::query(const Sphere3& sphere, NodeVisitor& visitor)
{
    for (int i = 0; i < m_nodes.size(); ++i)
    {
        RenderEntity* entity = m_nodes[i]->getEntity();
        if (entity->isUnbounded() || testSphereBox(sphere, entity->getWorldBounds()))
            visitor.visit(entity->getNode());
    }
}
void SimpleSpatialDS::query(const Ray3& ray, NodeVisitor& visitor)
{
    for (int i = 0; i < m_nodes.size(); ++i)
    {
        RenderEntity* entity = m_nodes[i]->getEntity();
        if (entity->isUnbounded() || testRayBox(ray, entity->getWorldBounds()))
            visitor.visit(entity->getNode());
    }
}
void SimpleSpatialDS::query(const Plane* planes, int numPlanes, NodeVisitor& visitor)
{
    for (int i = 0; i < m_nodes.size(); ++i)
    {
        RenderEntity* entity = m_nodes[i]->getEntity();
        if (entity->isUnbounded() || testBoxPlane(entity->getWorldBounds(), planes, numPlanes))
            visitor.visit(entity->getNode());
    }
}

// ---- QuadTreeSpatialDS ---------------------------------------------------------

// 0x0810ef20
QuadTreeSpatialDS::QTEntity::QTEntity(QuadTreeSpatialDS* ds, RenderEntity* entity)
    : SpatialNode(entity), m_pDS(ds), m_dirty(false), m_unbounded(true), m_level(0), m_index(0),
      m_pNext(0), m_pPrev(0)
{
    m_bounds.min.set(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    m_bounds.max.set(FLT_MAX, FLT_MAX, FLT_MAX);
}
// 0x0810f900: queue for re-insertion.
void QuadTreeSpatialDS::QTEntity::notifyBoundChanged()
{
    if (!m_dirty)
    {
        m_dirty = true;
        m_pDS->m_dirtyEnts.push_back(this);
    }
}

// 0x0810f6b0: levels hold 1, 4, 16 ... nodes stored contiguously.
QuadTreeSpatialDS::QuadTreeSpatialDS(float size, int levels) : m_size(size), m_numLevels(levels)
{
    int total = 0;
    for (int i = 0; i < levels; ++i)
        total += (1 << i) << i;
    m_pNodes = new QTNode[total];
    memset((void*)m_pNodes, 0, sizeof(QTNode) * total);
    m_ppLevels = new QTNode*[levels];
    int offset = 0;
    for (int i = 0; i < levels; ++i)
    {
        m_ppLevels[i] = m_pNodes + offset;
        offset += (1 << i) << i;
    }
}
// 0x0810f610
QuadTreeSpatialDS::~QuadTreeSpatialDS()
{
    delete[] m_pNodes;
    delete[] m_ppLevels;
}

// Bits of a 16-bit Morton number that come from x (even bits) and y (odd bits).
constexpr unsigned int MortonMaskX = 0x5555;
constexpr unsigned int MortonMaskY = 0xaaaa;
constexpr int MortonBitsPerAxis = 8;

// 0x0810df30: interleave the bits of x (even) and y (odd).
unsigned short QuadTreeSpatialDS::MortonNumber(int x, int y)
{
    unsigned int morton = 0;
    for (int i = 0; i < MortonBitsPerAxis; ++i)
    {
        morton |= ((x >> i) & 1) << (i * 2);
        morton |= ((y >> i) & 1) << (i * 2 + 1);
    }
    return (unsigned short)morton;
}

// 0x0810ef20 (RenderEntity overload)
SpatialNode* QuadTreeSpatialDS::addEntity(RenderEntity& entity)
{
    QTEntity* ent = new QTEntity(this, &entity);
    addEntity(ent);
    return ent;
}

// 0x0810e140: pick the deepest level whose cell still contains the entity extent,
// link into that node and grow the bounds of the node chain up to the root.
void QuadTreeSpatialDS::addEntity(QTEntity* ent)
{
    if (ent->m_unbounded)
    {
        m_unboundedEnts.push_back(ent);
        return;
    }
    const AABox3& b = ent->m_bounds;
    float extent = b.max.z - b.min.z;
    if (b.max.y - b.min.y > extent)
        extent = b.max.y - b.min.y;
    if (b.max.x - b.min.x > extent)
        extent = b.max.x - b.min.x;
    int level = 0;
    float cell = m_size;
    if (extent < m_size)
    {
        while (level < m_numLevels - 1)
        {
            cell *= 0.5f;
            ++level;
            if (cell <= extent)
                break;
        }
    }
    int maxIndex = (1 << level) - 1;
    int cx = (int)(((b.min.x + b.max.x) * 0.5f + m_size * 0.5f) / cell);
    int cz = (int)(((b.min.z + b.max.z) * 0.5f + m_size * 0.5f) / cell);
    unsigned int mx = 0, mz = 0;
    if (cx >= 0)
        mx = MortonNumber(cx < maxIndex ? cx : maxIndex, 0) & MortonMaskX;
    if (cz >= 0)
        mz = MortonNumber(0, cz < maxIndex ? cz : maxIndex) & MortonMaskY;
    int index = (int)(mx | mz);
    ent->m_level = level;
    ent->m_index = index;
    QTNode& n = node(level, index);
    ent->m_pPrev = 0;
    ent->m_pNext = n.pFirstEnt;
    if (n.pFirstEnt)
        n.pFirstEnt->m_pPrev = ent;
    n.pFirstEnt = ent;
    for (int l = level, i = index; l >= 0; --l, i >>= 2)
    {
        QTNode& p = node(l, i);
        if (p.numEnts++ == 0)
            p.bounds = b;
        else
            p.bounds.addBox(b);
    }
}
// 0x0810e000: unlink and mark the node chain dirty so bounds get recomputed lazily.
void QuadTreeSpatialDS::removeEntity(QTEntity* ent)
{
    if (ent->m_unbounded)
    {
        m_unboundedEnts.remove(ent);
        return;
    }
    QTNode& n = node(ent->m_level, ent->m_index);
    if (ent->m_pPrev)
        ent->m_pPrev->m_pNext = ent->m_pNext;
    if (ent->m_pNext)
        ent->m_pNext->m_pPrev = ent->m_pPrev;
    if (n.pFirstEnt == ent)
        n.pFirstEnt = ent->m_pNext;
    for (int l = ent->m_level, i = ent->m_index; l >= 0; --l, i >>= 2)
    {
        QTNode& p = node(l, i);
        p.numEnts--;
        p.dirty = true;
    }
}
// 0x0810f470
void QuadTreeSpatialDS::removeEntity(SpatialNode& spatialNode)
{
    QTEntity* ent = (QTEntity*)&spatialNode;
    m_dirtyEnts.remove(ent);
    removeEntity(ent);
    delete ent;
}
// 0x0810efa0
void QuadTreeSpatialDS::processDirtyEnts()
{
    for (int i = 0; i < m_dirtyEnts.size(); ++i)
    {
        QTEntity* ent = m_dirtyEnts[i];
        removeEntity(ent);
        ent->m_unbounded = ent->getEntity()->isUnbounded();
        if (!ent->m_unbounded)
            ent->m_bounds = ent->getEntity()->getWorldBounds();
        addEntity(ent);
        ent->m_dirty = false;
    }
    m_dirtyEnts.resize(0);
}
// 0x0810db90: bounds of a node = union of its entities and its (recomputed) children.
void QuadTreeSpatialDS::recomputeBounds(int level, int index)
{
    QTNode& n = node(level, index);
    n.bounds.min.set(FLT_MAX, FLT_MAX, FLT_MAX);
    n.bounds.max.set(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (QTEntity* e = n.pFirstEnt; e; e = e->m_pNext)
        n.bounds.addBox(e->m_bounds);
    if (level < m_numLevels - 1)
    {
        for (int c = 0; c < 4; ++c)
        {
            QTNode& child = node(level + 1, index * 4 + c);
            if (child.numEnts == 0)
                continue;
            if (child.dirty)
                recomputeBounds(level + 1, index * 4 + c);
            n.bounds.addBox(child.bounds);
        }
    }
    n.dirty = false;
}
// 0x0810def0
void QuadTreeSpatialDS::visitUnboundedEnts(NodeVisitor& visitor)
{
    for (int i = 0; i < m_unboundedEnts.size(); ++i)
        visitor.visit(m_unboundedEnts[i]->getEntity()->getNode());
}

#define QUADTREE_QUERY(ShapeType, shape, TEST)                                                     \
    void QuadTreeSpatialDS::query(ShapeType shape, NodeVisitor& visitor, int level, int index)     \
    {                                                                                              \
        QTNode* n = &node(level, index);                                                           \
        if (n->numEnts == 0 || !(TEST(n->bounds)))                                                 \
            return;                                                                                \
        if (n->dirty)                                                                              \
        {                                                                                          \
            recomputeBounds(level, index);                                                         \
            if (!(TEST(n->bounds)))                                                                \
                return;                                                                            \
        }                                                                                          \
        for (QTEntity* e = n->pFirstEnt; e; e = e->m_pNext)                                        \
            if (TEST(e->m_bounds))                                                                 \
                visitor.visit(e->getEntity()->getNode());                                          \
        if (level + 1 < m_numLevels)                                                               \
            for (int c = 0; c < 4; ++c)                                                            \
                query(shape, visitor, level + 1, index * 4 + c);                                   \
    }                                                                                              \
    void QuadTreeSpatialDS::query(ShapeType shape, NodeVisitor& visitor)                           \
    {                                                                                              \
        processDirtyEnts();                                                                        \
        visitUnboundedEnts(visitor);                                                               \
        query(shape, visitor, 0, 0);                                                               \
    }

#define TEST_BOX(b) testBoxBox(b, box)
#define TEST_SPHERE(b) testSphereBox(sphere, b)
#define TEST_RAY(b) testRayBox(ray, b)
// 0x0810ec70 / 0x0810f350
QUADTREE_QUERY(const AABox3&, box, TEST_BOX)
// 0x0810ea90 / 0x0810f2e0
QUADTREE_QUERY(const Sphere3&, sphere, TEST_SPHERE)
// 0x0810e8b0 / 0x0810f270
QUADTREE_QUERY(const Ray3&, ray, TEST_RAY)
#undef TEST_BOX
#undef TEST_SPHERE
#undef TEST_RAY
#undef QUADTREE_QUERY

// 0x0810e690
void QuadTreeSpatialDS::query(const Plane* planes, int numPlanes, NodeVisitor& visitor, int level,
                              int index)
{
    QTNode* n = &node(level, index);
    if (n->numEnts == 0 || !testBoxPlane(n->bounds, planes, numPlanes))
        return;
    if (n->dirty)
    {
        recomputeBounds(level, index);
        if (!testBoxPlane(n->bounds, planes, numPlanes))
            return;
    }
    for (QTEntity* e = n->pFirstEnt; e; e = e->m_pNext)
        if (testBoxPlane(e->m_bounds, planes, numPlanes))
            visitor.visit(e->getEntity()->getNode());
    if (level + 1 < m_numLevels)
        for (int c = 0; c < 4; ++c)
            query(planes, numPlanes, visitor, level + 1, index * 4 + c);
}
// 0x0810f1f0
void QuadTreeSpatialDS::query(const Plane* planes, int numPlanes, NodeVisitor& visitor)
{
    processDirtyEnts();
    visitUnboundedEnts(visitor);
    query(planes, numPlanes, visitor, 0, 0);
}
// 0x0810e5b0
void QuadTreeSpatialDS::drawNodes(int level)
{
    int count = (1 << level) << level;
    for (int i = 0; i < count; ++i)
    {
        QTNode& n = m_ppLevels[level][i];
        if (n.numEnts > 0)
            DebugDraw::drawBox(n.bounds.min - Vec3(0.1f, 0.1f, 0.1f),
                               n.bounds.max + Vec3(0.1f, 0.1f, 0.1f), Color::Green);
        for (QTEntity* e = n.pFirstEnt; e; e = e->m_pNext)
            DebugDraw::drawBox(e->m_bounds, Color::Yellow);
    }
}

} // namespace engine
