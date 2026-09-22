// Reconstructed from grimrock2.exe SpatialDS.cpp.
#include "engine/SpatialDS.h"
#include "core/Profiler.h"
#include "engine/RenderEntity.h"
#include <cfloat>
#include <cmath>
#include <cstring>

namespace engine
{

using namespace core;

// ---- SimpleSpatialDS -----------------------------------------------------------

// 0x004bf5f0
SimpleSpatialDS::SimpleSpatialDS() {}
// 0x004c0090
SimpleSpatialDS::~SimpleSpatialDS() {}
// 0x004bf610
void SimpleSpatialDS::addEntity(RenderEntity& entity)
{
    m_entities.push_back(&entity);
}
// 0x004bf640
void SimpleSpatialDS::removeEntity(RenderEntity& entity)
{
    m_entities.remove(&entity);
}
// 0x004bf800 / 0x004bf780 / 0x004bf700 / 0x004bf680: unbounded entities are always
// returned, the rest are tested against their world bounds.
void SimpleSpatialDS::query(const Plane* planes, int numPlanes, Array<RenderEntity*>& out)
{
    for (int i = 0; i < m_entities.size(); ++i)
    {
        RenderEntity* entity = m_entities[i];
        if (entity->isUnbounded() || testBoxPlane(entity->getWorldBounds(), planes, numPlanes))
            out.push_back(entity);
    }
}
void SimpleSpatialDS::query(const Ray3& ray, Array<RenderEntity*>& out)
{
    for (int i = 0; i < m_entities.size(); ++i)
    {
        RenderEntity* entity = m_entities[i];
        if (entity->isUnbounded() || testRayBox(ray, entity->getWorldBounds()))
            out.push_back(entity);
    }
}
void SimpleSpatialDS::query(const Sphere3& sphere, Array<RenderEntity*>& out)
{
    for (int i = 0; i < m_entities.size(); ++i)
    {
        RenderEntity* entity = m_entities[i];
        if (entity->isUnbounded() || testSphereBox(sphere, entity->getWorldBounds()))
            out.push_back(entity);
    }
}
void SimpleSpatialDS::query(const AABox3& box, Array<RenderEntity*>& out)
{
    for (int i = 0; i < m_entities.size(); ++i)
    {
        RenderEntity* entity = m_entities[i];
        if (entity->isUnbounded() || testBoxBox(entity->getWorldBounds(), box))
            out.push_back(entity);
    }
}

// ---- QuadTreeSpatialDS ---------------------------------------------------------

// 0x004c00d0: levels hold 1, 4, 16 ... nodes stored contiguously; every node links to
// its parent and first child by index.
QuadTreeSpatialDS::QuadTreeSpatialDS(float size, int levels) : m_size(size), m_numLevels(levels)
{
    int total = 0;
    for (int i = 0; i < levels; ++i)
        total += (1 << i) << i;
    m_pNodes = new QTNode[total];
    for (int i = 0; i < total; ++i)
    {
        m_pNodes[i].numEnts = 0;
        m_pNodes[i].parent = 0;
        m_pNodes[i].firstChild = 0;
        m_pNodes[i].dirty = true;
    }
    int offset = 0;
    for (int i = 0; i < levels; ++i)
    {
        m_levelStart.push_back((unsigned short)offset);
        offset += (1 << i) << i;
    }
    offset = 0;
    for (int level = 0; level < levels; ++level)
    {
        int count = (1 << level) << level;
        for (int i = 0; i < count; ++i)
        {
            QTNode& n = m_pNodes[offset + i];
            n.parent = level > 0 ? (unsigned short)(m_levelStart[level - 1] + (i >> 2)) : NoNode;
            n.firstChild =
                level < levels - 1 ? (unsigned short)(m_levelStart[level + 1] + i * 4) : NoNode;
        }
        offset += count;
    }
}
// 0x004c02e0
QuadTreeSpatialDS::~QuadTreeSpatialDS()
{
    delete[] m_pNodes;
}

// 0x004bef10: the classic magic number bit spreading, 8 bits per axis
unsigned int QuadTreeSpatialDS::MortonNumber(int x, int y)
{
    unsigned int morton = 0;
    for (int i = 0; i < 8; ++i)
    {
        morton |= ((x >> i) & 1) << (i * 2);
        morton |= ((y >> i) & 1) << (i * 2 + 1);
    }
    return morton;
}

// 0x004c0390: pick the deepest level whose cell still contains the entity extent, append
// to that node and grow the bounds of the node chain up to the root.
void QuadTreeSpatialDS::addEntity(RenderEntity& entity)
{
    if (entity.isUnbounded())
    {
        entity.setSpatialIndex(Unbounded);
        m_unboundedEnts.push_back(&entity);
        return;
    }
    AABox3 b = entity.getWorldBounds();
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
    cx = cx < 0 ? 0 : (cx > maxIndex ? maxIndex : cx);
    cz = cz < 0 ? 0 : (cz > maxIndex ? maxIndex : cz);
    int nodeIndex = (int)MortonNumber(cx, cz) + m_levelStart[level];
    QTNode* n = &m_pNodes[nodeIndex];
    entity.setSpatialIndex((nodeIndex << 16) | n->entries.size());
    Entry entry;
    entry.pEntity = &entity;
    entry.bounds = b;
    n->entries.push_back(entry);
    for (;;)
    {
        if (n->numEnts++ == 0)
            n->bounds = b;
        else
            n->bounds.addBox(b);
        if (n->parent == NoNode)
            break;
        n = &m_pNodes[n->parent];
    }
}
// 0x004bf8d0: the last entry of the node takes the slot; the node chain is marked
// dirty so the bounds get recomputed lazily.
void QuadTreeSpatialDS::removeEntity(RenderEntity& entity)
{
    m_dirtyEnts.remove(&entity);
    int index = entity.getSpatialIndex();
    if (index == NotInTree)
        return;
    if (index == Unbounded)
    {
        entity.setSpatialIndex(NotInTree);
        m_unboundedEnts.remove(&entity);
        return;
    }
    QTNode* n = &m_pNodes[(unsigned int)index >> 16];
    int slot = index & 0xffff;
    n->entries[slot] = n->entries[n->entries.size() - 1];
    n->entries[slot].pEntity->setSpatialIndex(index);
    if (n->entries.size() > 0)
        n->entries.resize(n->entries.size() - 1);
    entity.setSpatialIndex(NotInTree);
    for (;;)
    {
        --n->numEnts;
        n->dirty = true;
        if (n->parent == NoNode)
            break;
        n = &m_pNodes[n->parent];
    }
}
// 0x004bf9d0
void QuadTreeSpatialDS::notifyBoundChanged(RenderEntity& entity)
{
    if (entity.getSpatialIndex() == NotInTree)
        return;
    removeEntity(entity);
    m_dirtyEnts.push_back(&entity);
}
// 0x004bfa10
void QuadTreeSpatialDS::prepare()
{
    ProfileScope profile("_PrepareSpatialDS");
    for (int i = 0; i < m_dirtyEnts.size(); ++i)
        addEntity(*m_dirtyEnts[i]);
    m_dirtyEnts.clear();
    if (m_pNodes[0].dirty)
        recomputeBounds(0);
}
// 0x004bf330: bounds of the entries plus the (recomputed) child bounds.
void QuadTreeSpatialDS::recomputeBounds(int nodeIndex)
{
    QTNode& n = m_pNodes[nodeIndex];
    n.bounds.min.set(FLT_MAX, FLT_MAX, FLT_MAX);
    n.bounds.max.set(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (int i = 0; i < n.entries.size(); ++i)
        n.bounds.addBox(n.entries[i].bounds);
    if (n.firstChild != NoNode)
    {
        for (int i = 0; i < 4; ++i)
        {
            int child = n.firstChild + i;
            if (m_pNodes[child].dirty)
                recomputeBounds(child);
            n.bounds.addBox(m_pNodes[child].bounds);
        }
    }
    n.dirty = false;
}
// 0x004bfff0
void QuadTreeSpatialDS::addUnboundedEntities(Array<RenderEntity*>& out)
{
    for (int i = 0; i < m_unboundedEnts.size(); ++i)
        out.push_back(m_unboundedEnts[i]);
}

// 0x004c0690 / 0x004c06c0 / 0x004c06f0 / 0x004c0720
void QuadTreeSpatialDS::query(const AABox3& box, Array<RenderEntity*>& out)
{
    prepare();
    addUnboundedEntities(out);
    query(box, out, 0);
}
void QuadTreeSpatialDS::query(const Sphere3& sphere, Array<RenderEntity*>& out)
{
    prepare();
    addUnboundedEntities(out);
    query(sphere, out, 0);
}
void QuadTreeSpatialDS::query(const Ray3& ray, Array<RenderEntity*>& out)
{
    prepare();
    addUnboundedEntities(out);
    query(ray, out, 0);
}
void QuadTreeSpatialDS::query(const Plane* planes, int numPlanes, Array<RenderEntity*>& out)
{
    prepare();
    addUnboundedEntities(out);
    if (numPlanes == 6)
        queryFrustum(planes, out, 0);
    else
        query(planes, numPlanes, out, 0);
}

// 0x004bfae0 / 0x004bfbe0 / 0x004bfce0 / 0x004bfde0 / 0x004bfef0: skip empty subtrees and
// subtrees whose bounds miss the volume; test the entries, then recurse into the children.
void QuadTreeSpatialDS::query(const AABox3& box, Array<RenderEntity*>& out, int nodeIndex)
{
    QTNode& n = m_pNodes[nodeIndex];
    if (n.numEnts == 0 || !testBoxBox(n.bounds, box))
        return;
    for (int i = 0; i < n.entries.size(); ++i)
        if (testBoxBox(n.entries[i].bounds, box))
            out.push_back(n.entries[i].pEntity);
    if (n.firstChild == NoNode)
        return;
    for (int i = 0; i < 4; ++i)
        query(box, out, n.firstChild + i);
}
void QuadTreeSpatialDS::query(const Sphere3& sphere, Array<RenderEntity*>& out, int nodeIndex)
{
    QTNode& n = m_pNodes[nodeIndex];
    if (n.numEnts == 0 || !testSphereBox(sphere, n.bounds))
        return;
    for (int i = 0; i < n.entries.size(); ++i)
        if (testSphereBox(sphere, n.entries[i].bounds))
            out.push_back(n.entries[i].pEntity);
    if (n.firstChild == NoNode)
        return;
    for (int i = 0; i < 4; ++i)
        query(sphere, out, n.firstChild + i);
}
void QuadTreeSpatialDS::query(const Ray3& ray, Array<RenderEntity*>& out, int nodeIndex)
{
    QTNode& n = m_pNodes[nodeIndex];
    if (n.numEnts == 0 || !testRayBox(ray, n.bounds))
        return;
    for (int i = 0; i < n.entries.size(); ++i)
        if (testRayBox(ray, n.entries[i].bounds))
            out.push_back(n.entries[i].pEntity);
    if (n.firstChild == NoNode)
        return;
    for (int i = 0; i < 4; ++i)
        query(ray, out, n.firstChild + i);
}
void QuadTreeSpatialDS::query(const Plane* planes, int numPlanes, Array<RenderEntity*>& out,
                              int nodeIndex)
{
    QTNode& n = m_pNodes[nodeIndex];
    if (n.numEnts == 0 || !testBoxPlane(n.bounds, planes, numPlanes))
        return;
    for (int i = 0; i < n.entries.size(); ++i)
        if (testBoxPlane(n.entries[i].bounds, planes, numPlanes))
            out.push_back(n.entries[i].pEntity);
    if (n.firstChild == NoNode)
        return;
    for (int i = 0; i < 4; ++i)
        query(planes, numPlanes, out, n.firstChild + i);
}
// 0x004bf040: box against six planes, centre/extent form
static bool testBoxFrustum(const AABox3& box, const Plane* planes)
{
    Vec3 center = (box.min + box.max) * 0.5f;
    Vec3 extent = box.max - center;
    for (int i = 0; i < 6; ++i)
    {
        const Plane& p = planes[i];
        float d = p.normal.x * center.x + p.normal.y * center.y + p.normal.z * center.z - p.d;
        float r = fabsf(p.normal.x) * extent.x + fabsf(p.normal.y) * extent.y +
                  fabsf(p.normal.z) * extent.z;
        if (d <= -r)
            return false;
    }
    return true;
}
void QuadTreeSpatialDS::queryFrustum(const Plane* planes, Array<RenderEntity*>& out, int nodeIndex)
{
    QTNode& n = m_pNodes[nodeIndex];
    if (n.numEnts == 0 || !testBoxFrustum(n.bounds, planes))
        return;
    for (int i = 0; i < n.entries.size(); ++i)
        if (testBoxFrustum(n.entries[i].bounds, planes))
            out.push_back(n.entries[i].pEntity);
    if (n.firstChild == NoNode)
        return;
    for (int i = 0; i < 4; ++i)
        queryFrustum(planes, out, n.firstChild + i);
}

} // namespace engine
