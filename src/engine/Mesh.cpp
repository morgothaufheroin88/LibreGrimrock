// Reconstructed from Grimrock.bin.x86 Mesh.cpp.
#include "engine/Mesh.h"
#include "core/Exception.h"
#include "core/FileStream.h"
#include "core/HashMap.h"
#include "core/Math.h"
#include "core/Sys.h"
#include "engine/AssetProcessor.h"
#include "engine/Material.h"
#include "engine/Model.h"
#include "engine/Renderer.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace engine
{

constexpr unsigned int MeshMagic = 0x4853454d; // "MESH"

using namespace core;

Array<Mesh*> Mesh::sm_meshes;
Array<RenderableMesh*> RenderableMesh::sm_meshes;

// 0x080e1eb0
Mesh::Mesh(int numVertices) : m_numVertices(numVertices), m_boundingSphereRadius(0.0f)
{
    sm_meshes.push_back(this);
    memset(m_vertexArrays, 0, sizeof(m_vertexArrays));
    m_bounds.min.set(0, 0, 0);
    m_bounds.max.set(0, 0, 0);
}
// 0x080e11e0
Mesh::Mesh(const Mesh& other)
    : m_filename(other.m_filename), m_numVertices(other.m_numVertices), m_indices(other.m_indices),
      m_segments(other.m_segments), m_boundingSphereCenter(other.m_boundingSphereCenter),
      m_boundingSphereRadius(other.m_boundingSphereRadius), m_bounds(other.m_bounds)
{
    sm_meshes.push_back(this);
    memset(m_vertexArrays, 0, sizeof(m_vertexArrays));
    for (int i = 0; i < NumVertexArrays; ++i)
    {
        const VertexArray& src = other.m_vertexArrays[i];
        if (src.pData)
            setVertexArray(i, src.type, src.components, src.pData);
    }
}
// 0x080e1c20
Mesh::~Mesh()
{
    for (int i = 0; i < NumVertexArrays; ++i)
        if (m_vertexArrays[i].pData)
            delete[] (char*)m_vertexArrays[i].pData;
    sm_meshes.remove(this);
}
// 0x080dd4b0
void Mesh::setNumVertices(int n)
{
    m_numVertices = n;
}
// 0x080de520 type table: byte=1, short=2, int=4, float=4
int Mesh::getVertexTypeSize(int type)
{
    static constexpr int table[][2] = {{TypeByte, 1}, {TypeShort, 2}, {TypeInt, 4}, {TypeFloat, 4}};
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); ++i)
        if (table[i][0] == type)
            return table[i][1];
    throw Exception("Invalid vertex array type");
}
// 0x080de520
void Mesh::setVertexArray(int index, int type, int components, const void* data)
{
    VertexArray& a = m_vertexArrays[index];
    a.type = type;
    a.components = components;
    a.stride = getVertexTypeSize(type) * components;
    int bytes = a.stride * m_numVertices;
    if (a.pData)
        delete[] (char*)a.pData;
    a.pData = 0;
    if (bytes > 0 && data)
    {
        a.pData = new char[bytes];
        memcpy(a.pData, data, bytes);
    }
    if (index == Position)
        updateBounds();
}
// 0x080dfc20
void Mesh::setIndices(const int* indices, int count)
{
    m_indices.resize(0);
    m_indices.reserve(count);
    for (int i = 0; i < count; ++i)
        m_indices.push_back(indices[i]);
}
// 0x080dfd20: new segments use the default material.
MeshSegment& Mesh::addSegment()
{
    MeshSegment seg;
    seg.material = Material::Default;
    seg.primitiveType = TriangleList;
    seg.firstIndex = 0;
    seg.numTriangles = 0;
    m_segments.push_back(seg);
    return m_segments.back();
}
// 0x080ded30
void Mesh::clearSegments()
{
    m_segments.clear();
}
// 0x080ddf90: axis aligned box plus a bounding sphere around its centre.
void Mesh::updateBounds()
{
    const Vec3* positions = (const Vec3*)m_vertexArrays[Position].pData;
    if (!positions)
    {
        m_bounds.min.set(0, 0, 0);
        m_bounds.max.set(0, 0, 0);
        m_boundingSphereCenter.set(0, 0, 0);
        m_boundingSphereRadius = 0.0f;
        return;
    }
    Vec3 boundsMin(FLT_MAX, FLT_MAX, FLT_MAX);
    Vec3 boundsMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (int i = 0; i < m_numVertices; ++i)
    {
        boundsMin = minVec(boundsMin, positions[i]);
        boundsMax = maxVec(boundsMax, positions[i]);
    }
    Vec3 center = (boundsMin + boundsMax) * 0.5f;
    float maxSqr = 0.0f;
    for (int i = 0; i < m_numVertices; ++i)
    {
        float sqrDist = (center - positions[i]).sqrLength();
        if (sqrDist > maxSqr)
            maxSqr = sqrDist;
    }
    m_bounds.min = boundsMin;
    m_bounds.max = boundsMax;
    m_boundingSphereCenter = center;
    m_boundingSphereRadius = std::sqrt(maxSqr);
}
// 0x080dedb0
void Mesh::computeVertexNormals()
{
    const Vec3* positions = (const Vec3*)m_vertexArrays[Position].pData;
    Vec3* normals = new Vec3[m_numVertices];
    for (int i = 0; i < m_numVertices; ++i)
        normals[i].set(0, 0, 0);
    // per segment; triangle lists and quads (the first three corners) only; unit face
    // normals of (c - a) x (b - a) accumulated per vertex
    for (int s = 0; s < m_segments.size(); ++s)
    {
        const MeshSegment& seg = m_segments[s];
        if (seg.primitiveType != 2 && seg.primitiveType != 3)
            continue;
        const int* idx = m_indices.data() + seg.firstIndex;
        int step = seg.primitiveType + 1;
        for (int k = 0; k < seg.numTriangles; ++k, idx += step)
        {
            int a = idx[0], b = idx[1], c = idx[2];
            Vec3 faceNormal = cross(positions[c] - positions[a], positions[b] - positions[a]);
            faceNormal = faceNormal * (1.0f / std::sqrt(dot(faceNormal, faceNormal)));
            normals[a] += faceNormal;
            normals[b] += faceNormal;
            normals[c] += faceNormal;
        }
    }
    for (int i = 0; i < m_numVertices; ++i)
        if (dot(normals[i], normals[i]) > 0.0f)
            normals[i].normalize();
    setVertexArray(Normal, TypeFloat, 3, normals);
    delete[] normals;
}
// 0x080df090: per triangle tangent frame accumulated per vertex.
void Mesh::computeTangentVectors()
{
    const char* positions = (const char*)m_vertexArrays[Position].pData;
    const char* uv = (const char*)m_vertexArrays[Texcoord0].pData;
    int uvStride = m_vertexArrays[Texcoord0].components * 4;
    if (!uv)
        throw Exception("No texcoords - can't compute tangents");
    Vec3* tangents = new Vec3[m_numVertices];
    Vec3* bitangents = new Vec3[m_numVertices];
    for (int i = 0; i < m_numVertices; ++i)
    {
        tangents[i].set(0, 0, 0);
        bitangents[i].set(0, 0, 0);
    }
    // per segment; triangle lists and quads (the first three corners) only
    for (int s = 0; s < m_segments.size(); ++s)
    {
        const MeshSegment& seg = m_segments[s];
        if (seg.primitiveType != 2 && seg.primitiveType != 3)
            continue;
        const int* idx = m_indices.data() + seg.firstIndex;
        int step = seg.primitiveType + 1;
        for (int k = 0; k < seg.numTriangles; ++k, idx += step)
        {
            int a = idx[0], b = idx[1], c = idx[2];
            const Vec3& pa = *(const Vec3*)(positions + a * 12);
            const Vec3& pb = *(const Vec3*)(positions + b * 12);
            const Vec3& pc = *(const Vec3*)(positions + c * 12);
            const Vec2& ta = *(const Vec2*)(uv + a * uvStride);
            const Vec2& tb = *(const Vec2*)(uv + b * uvStride);
            const Vec2& tc = *(const Vec2*)(uv + c * uvStride);
            Vec3 e1 = pb - pa, e2 = pc - pa;
            float s1 = tb.x - ta.x, s2 = tc.x - ta.x;
            float t1 = tb.y - ta.y, t2 = tc.y - ta.y;
            float invDet = 1.0f / (s1 * t2 - s2 * t1);
            Vec3 sdir((e1.x * t2 - e2.x * t1) * invDet, (e1.y * t2 - e2.y * t1) * invDet,
                      (e1.z * t2 - e2.z * t1) * invDet);
            Vec3 tdir(-((s1 * e2.x - s2 * e1.x) * invDet), -((s1 * e2.y - s2 * e1.y) * invDet),
                      -((s1 * e2.z - s2 * e1.z) * invDet));
            tangents[a] += sdir;
            tangents[b] += sdir;
            tangents[c] += sdir;
            bitangents[a] += tdir;
            bitangents[b] += tdir;
            bitangents[c] += tdir;
        }
    }
    for (int i = 0; i < m_numVertices; ++i)
    {
        if (dot(tangents[i], tangents[i]) > 0.0f)
            tangents[i].normalize();
        if (dot(bitangents[i], bitangents[i]) > 0.0f)
            bitangents[i].normalize();
    }
    setVertexArray(Tangent, TypeFloat, 3, tangents);
    setVertexArray(Bitangent, TypeFloat, 3, bitangents);
    delete[] tangents;
    delete[] bitangents;
}
// 0x080dd4c0
void Mesh::normalizeBoneWeights()
{
    VertexArray& a = m_vertexArrays[BoneWeights];
    if (!a.pData)
        return;
    for (int i = 0; i < m_numVertices; ++i)
    {
        float* w = (float*)((char*)a.pData + i * a.stride);
        float sum = 0.0f;
        for (int c = 0; c < a.components; ++c)
            sum += w[c];
        if (sum > 0.0f)
            for (int c = 0; c < a.components; ++c)
                w[c] /= sum;
    }
}
// 0x080dd560
void Mesh::flipFaces()
{
    // per segment: triangles swap their first two corners, quads their first and third
    for (int s = 0; s < m_segments.size(); ++s)
    {
        const MeshSegment& seg = m_segments[s];
        int* idx = m_indices.data() + seg.firstIndex;
        if (seg.primitiveType == 2)
        {
            for (int k = 0; k < seg.numTriangles; ++k, idx += 3)
                swap(idx[0], idx[1]);
        }
        else if (seg.primitiveType == 3)
        {
            for (int k = 0; k < seg.numTriangles; ++k, idx += 4)
                swap(idx[0], idx[2]);
        }
    }
}
// 0x080df7a0: segments already hold triangle lists.
void Mesh::triangulate() {}
// 0x080e4cf0
void Mesh::scale(float s)
{
    Vec3* p = (Vec3*)m_vertexArrays[Position].pData;
    if (!p)
        return;
    for (int i = 0; i < m_numVertices; ++i)
        p[i] *= s;
    updateBounds();
}
// 0x080e4f80
void Mesh::translate(const Vec3& t)
{
    Vec3* p = (Vec3*)m_vertexArrays[Position].pData;
    if (!p)
        return;
    for (int i = 0; i < m_numVertices; ++i)
        p[i] += t;
    updateBounds();
}
// 0x080e6480: positions by the matrix, normal/tangent/bitangent by its inverse transpose.
void Mesh::transform(const Matrix4x3& m)
{
    Vec3* p = (Vec3*)m_vertexArrays[Position].pData;
    if (p)
        for (int i = 0; i < m_numVertices; ++i)
            p[i] = m.transformPoint(p[i]);
    Matrix3x3 nm = m.rotation();
    nm.invert();
    nm.transpose();
    for (int arr = Normal; arr <= Bitangent; ++arr)
    {
        Vec3* v = (Vec3*)m_vertexArrays[arr].pData;
        if (!v)
            continue;
        for (int i = 0; i < m_numVertices; ++i)
        {
            v[i] = nm.transform(v[i]);
            v[i] = v[i] * (1.0f / std::sqrt(dot(v[i], v[i])));
        }
    }
    updateBounds();
}
// 0x080e6990
void Mesh::rotate(const Matrix3x3& m)
{
    transform(Matrix4x3(m, Vec3(0, 0, 0)));
}
// 0x080de6c0: closest triangle hit.
bool Mesh::raycast(const Ray3& ray, float& tOut, int* triangle, Vec3* normal)
{
    const Vec3* positions = (const Vec3*)m_vertexArrays[Position].pData;
    if (!positions)
        return false;
    bool hit = false;
    float nearest = FLT_MAX;
    for (int i = 0; i + 2 < m_indices.size(); i += 3)
    {
        int a = m_indices[i], b = m_indices[i + 1], c = m_indices[i + 2];
        float t, u, v;
        if (intersectRayTriangle(ray, positions[a], positions[b], positions[c], t, u, v, 0.0f) &&
            t < nearest)
        {
            nearest = t;
            hit = true;
            if (triangle)
                *triangle = i / 3;
            if (normal)
            {
                *normal = cross(positions[c] - positions[a], positions[b] - positions[a]);
                normal->normalize();
            }
        }
    }
    if (hit)
        tOut = nearest;
    return hit;
}
// 0x080de870
bool Mesh::raycast(const Ray3& ray)
{
    float distance;
    return raycast(ray, distance, 0, 0);
}
// 0x080de1f0: memcmp over every present vertex array.
int Mesh::compareVertex(const Mesh& mesh, int a, int b)
{
    for (int i = 0; i < NumVertexArrays; ++i)
    {
        const VertexArray& va = mesh.m_vertexArrays[i];
        if (!va.pData)
            continue;
        int order = memcmp((const char*)va.pData + a * va.stride,
                           (const char*)va.pData + b * va.stride, va.stride);
        if (order != 0)
            return order < 0 ? -1 : 1;
    }
    return 0;
}
struct Mesh::SortVertex
{
    const Mesh* mesh;
    int index;
    bool operator<(const SortVertex& o) const
    {
        return compareVertex(*mesh, index, o.index) < 0;
    }
};
// 0x080dfec0: sort vertices, collapse equal runs and remap the indices.
void Mesh::weldVertices()
{
    if (m_numVertices == 0)
        return;
    Array<SortVertex> sorted(m_numVertices);
    for (int i = 0; i < m_numVertices; ++i)
    {
        sorted[i].mesh = this;
        sorted[i].index = i;
    }
    std::sort(sorted.begin(), sorted.end());
    Array<int> remap(m_numVertices);
    Array<int> unique;
    for (int i = 0; i < m_numVertices; ++i)
    {
        if (i == 0 || compareVertex(*this, sorted[i - 1].index, sorted[i].index) != 0)
            unique.push_back(sorted[i].index);
        remap[sorted[i].index] = unique.size() - 1;
    }
    for (int i = 0; i < m_indices.size(); ++i)
        m_indices[i] = remap[m_indices[i]];
    int newCount = unique.size();
    debugPrint("weld vertices %d -> %d\n", m_numVertices, newCount);
    for (int arr = 0; arr < NumVertexArrays; ++arr)
    {
        VertexArray& va = m_vertexArrays[arr];
        if (!va.pData)
            continue;
        char* data = new char[newCount * va.stride];
        for (int i = 0; i < newCount; ++i)
            memcpy(data + i * va.stride, (char*)va.pData + unique[i] * va.stride, va.stride);
        delete[] (char*)va.pData;
        va.pData = data;
    }
    m_numVertices = newCount;
    updateBounds();
}
// 0x080e7740
bool Mesh::sortByMaterial(const MeshSegment& a, const MeshSegment& b)
{
    return a.material.get() < b.material.get();
}
// 0x080e09b0: sort segments by material and merge neighbours sharing a material.
void Mesh::optimizeSegments()
{
    if (m_segments.size() == 0)
        return;
    std::sort(m_segments.begin(), m_segments.end(), sortByMaterial);
    Array<int> newIndices;
    Array<MeshSegment> merged;
    for (int i = 0; i < m_segments.size(); ++i)
    {
        const MeshSegment& segment = m_segments[i];
        if (merged.size() == 0 || merged.back().material != segment.material ||
            merged.back().primitiveType != segment.primitiveType)
        {
            MeshSegment mergedSegment = segment;
            mergedSegment.firstIndex = newIndices.size();
            mergedSegment.numTriangles = 0;
            merged.push_back(mergedSegment);
        }
        for (int k = 0; k < segment.numTriangles * 3; ++k)
            newIndices.push_back(m_indices[segment.firstIndex + k]);
        merged.back().numTriangles += segment.numTriangles;
    }
    debugPrint("optimize segments %d -> %d\n", m_segments.size(), merged.size());
    m_indices = newIndices;
    m_segments = merged;
}
// 0x080e5220
Mesh* Mesh::mergeMeshes(const Array<Mesh*>& meshes, const Array<Matrix4x3>* transforms)
{
    int totalVertices = 0;
    for (int i = 0; i < meshes.size(); ++i)
        totalVertices += meshes[i]->m_numVertices;
    Mesh* result = new Mesh(totalVertices);
    for (int arr = 0; arr < NumVertexArrays; ++arr)
    {
        const VertexArray* proto = 0;
        for (int i = 0; i < meshes.size() && !proto; ++i)
            if (meshes[i]->m_vertexArrays[arr].pData)
                proto = &meshes[i]->m_vertexArrays[arr];
        if (!proto)
            continue;
        char* data = new char[totalVertices * proto->stride];
        memset(data, 0, totalVertices * proto->stride);
        int offset = 0;
        for (int i = 0; i < meshes.size(); ++i)
        {
            Mesh copy(*meshes[i]);
            if (transforms)
                copy.transform((*transforms)[i]);
            const VertexArray& va = copy.m_vertexArrays[arr];
            if (va.pData && va.stride == proto->stride)
                memcpy(data + offset * proto->stride, va.pData, copy.m_numVertices * va.stride);
            offset += copy.m_numVertices;
        }
        result->setVertexArray(arr, proto->type, proto->components, data);
        delete[] data;
    }
    int vertexOffset = 0;
    for (int i = 0; i < meshes.size(); ++i)
    {
        const Mesh* mesh = meshes[i];
        int indexOffset = result->m_indices.size();
        for (int k = 0; k < mesh->m_indices.size(); ++k)
            result->m_indices.push_back(mesh->m_indices[k] + vertexOffset);
        for (int k = 0; k < mesh->m_segments.size(); ++k)
        {
            MeshSegment segment = mesh->m_segments[k];
            segment.firstIndex += indexOffset;
            result->m_segments.push_back(segment);
        }
        vertexOffset += mesh->m_numVertices;
    }
    result->updateBounds();
    return result;
}
// 0x080dde40
Mesh* Mesh::getMeshByFilename(const char* filename)
{
    for (int i = 0; i < sm_meshes.size(); ++i)
        if (strcmp(sm_meshes[i]->m_filename.c_str(), filename) == 0)
            return sm_meshes[i];
    return 0;
}

// ---- RenderableMesh --------------------------------------------------------------

// 0x080ddeb0
RenderableMesh::RenderableMesh()
{
    sm_meshes.push_back(this);
}
// 0x080de910
RenderableMesh::~RenderableMesh()
{
    sm_meshes.remove(this);
}
// 0x080dddd0
RenderableMesh* RenderableMesh::getRenderableMeshByFilename(const char* filename)
{
    for (int i = 0; i < sm_meshes.size(); ++i)
        if (strcmp(sm_meshes[i]->m_filename.c_str(), filename) == 0)
            return sm_meshes[i];
    return 0;
}
// 0x080ddcd0
RenderableMesh* createRenderableMesh(Mesh& mesh, bool keepSourceData)
{
    RenderableMesh* renderable = Renderer::getActiveRenderer()->createRenderableMesh();
    renderable->init(mesh, keepSourceData);
    return renderable;
}
// 0x080deaf0
RenderableMesh* loadRenderableMesh(const char* filename, bool keepSourceData)
{
    RenderableMesh* existing = RenderableMesh::getRenderableMeshByFilename(filename);
    if (existing)
        return existing;
    SharedPtr<Mesh> mesh(loadMesh(filename));
    RenderableMesh* renderable = Renderer::getActiveRenderer()->createRenderableMesh();
    renderable->setFilename(filename);
    renderable->init(*mesh, keepSourceData);
    return renderable;
}

// ---- file format ------------------------------------------------------------------

// 0x080e3b40
Mesh* loadMesh(InputStream& stream)
{
    unsigned int magic, version, numVertices;
    stream.readInt(magic);
    if (magic != MeshMagic)
        throw InvalidFileFormatException("Invalid file format: %s", stream.getFilename());
    stream.readInt(version);
    if (version != 2)
        throw InvalidFileVersionException("Invalid file version: %s", stream.getFilename());
    stream.readInt(numVertices);
    Mesh* mesh = new Mesh((int)numVertices);
    for (int i = 0; i < Mesh::NumVertexArrays; ++i)
    {
        unsigned int type, components, stride;
        stream.readInt(type);
        stream.readInt(components);
        stream.readInt(stride);
        if (stride == 0)
        {
            mesh->setVertexArray(i, (int)type, (int)components, 0);
        }
        else
        {
            int bytes = (int)(stride * numVertices);
            char* data = new char[bytes];
            stream.readBytes(data, bytes);
            mesh->setVertexArray(i, (int)type, (int)components, data);
            delete[] data;
        }
    }
    unsigned int numIndices;
    stream.readInt(numIndices);
    int* indices = new int[numIndices];
    stream.readBytes(indices, (int)numIndices * 4);
    mesh->setIndices(indices, (int)numIndices);
    delete[] indices;
    unsigned int numSegments;
    stream.readInt(numSegments);
    mesh->clearSegments();
    for (unsigned int i = 0; i < numSegments; ++i)
    {
        MeshSegment& seg = mesh->addSegment();
        String materialName;
        stream.readString(materialName);
        seg.material.reset(Material::getMaterialByName(materialName.c_str()));
        stream.readInt(seg.primitiveType);
        stream.readInt(seg.firstIndex);
        stream.readInt(seg.numTriangles);
    }
    Vec3 center;
    float radius;
    AABox3 box;
    stream.readVector3(center);
    stream.readFloat(radius);
    stream.readVector3(box.min);
    stream.readVector3(box.max);
    mesh->setBounds(box, center, radius);
    return mesh;
}
// 0x080de9c0
Mesh* loadMesh(const char* filename)
{
    Mesh* existing = Mesh::getMeshByFilename(filename);
    if (existing)
        return existing;
    AssetProcessor* processor = findAssetProcessor(AssetProcessor::ModelAsset, filename);
    processor->processFile(filename);
    String native = processor->getNativeFile(filename);
    FileInputStream stream(native.c_str());
    Mesh* mesh = meshLoader(stream);
    mesh->setFilename(filename);
    return mesh;
}
// 0x080dd660
void saveMesh(Mesh& mesh, OutputStream& stream)
{
    stream.writeInt(0x4853454du);
    stream.writeInt(2u);
    stream.writeInt((unsigned int)mesh.getNumVertices());
    for (int i = 0; i < Mesh::NumVertexArrays; ++i)
    {
        const Mesh::VertexArray& va = mesh.getVertexArrayInfo(i);
        stream.writeInt((unsigned int)va.type);
        stream.writeInt((unsigned int)va.components);
        stream.writeInt((unsigned int)(va.pData ? va.stride : 0));
        if (va.pData)
            stream.writeBytes(va.pData, va.stride * mesh.getNumVertices());
    }
    stream.writeInt((unsigned int)mesh.getIndices().size());
    stream.writeBytes(mesh.getIndices().data(), mesh.getIndices().size() * 4);
    stream.writeInt((unsigned int)mesh.getNumSegments());
    for (int i = 0; i < mesh.getNumSegments(); ++i)
    {
        const MeshSegment& seg = mesh.getSegment(i);
        stream.writeString(seg.material ? seg.material->getName() : String(""));
        stream.writeInt(seg.primitiveType);
        stream.writeInt(seg.firstIndex);
        stream.writeInt(seg.numTriangles);
    }
    stream.writeVector3(mesh.getBoundingSphereCenter());
    stream.writeFloat(mesh.getBoundingSphereRadius());
    stream.writeVector3(mesh.getBoundingBox().min);
    stream.writeVector3(mesh.getBoundingBox().max);
}

// ---- procedural meshes -----------------------------------------------------------

static void finishMesh(Mesh* mesh, const Array<int>& indices)
{
    mesh->setIndices(indices.data(), indices.size());
    MeshSegment& seg = mesh->addSegment();
    seg.firstIndex = 0;
    seg.numTriangles = indices.size() / 3;
}

// 0x080e6e40: 24 vertices, 6 faces with per face normals and 0..1 texcoords.
Mesh* createBox(float w, float h, float d)
{
    Vec3 half(w * 0.5f, h * 0.5f, d * 0.5f);
    static constexpr float faces[6][3] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                                          {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
    Mesh* mesh = new Mesh(24);
    Vec3 pos[24], nrm[24];
    Vec2 uv[24];
    Array<int> idx;
    for (int f = 0; f < 6; ++f)
    {
        Vec3 n(faces[f][0], faces[f][1], faces[f][2]);
        Vec3 u = std::fabs(n.y) > 0.5f ? Vec3(1, 0, 0) : cross(Vec3(0, 1, 0), n);
        Vec3 v = cross(n, u);
        for (int k = 0; k < 4; ++k)
        {
            float su = (k == 1 || k == 2) ? 1.0f : -1.0f;
            float sv = (k >= 2) ? 1.0f : -1.0f;
            Vec3 corner = n + u * su + v * sv;
            pos[f * 4 + k] = Vec3(corner.x * half.x, corner.y * half.y, corner.z * half.z);
            nrm[f * 4 + k] = n;
            uv[f * 4 + k] = Vec2(su * 0.5f + 0.5f, sv * 0.5f + 0.5f);
        }
        idx.push_back(f * 4 + 0);
        idx.push_back(f * 4 + 1);
        idx.push_back(f * 4 + 2);
        idx.push_back(f * 4 + 0);
        idx.push_back(f * 4 + 2);
        idx.push_back(f * 4 + 3);
    }
    mesh->setVertexArray(Mesh::Position, Mesh::TypeFloat, 3, pos);
    mesh->setVertexArray(Mesh::Normal, Mesh::TypeFloat, 3, nrm);
    mesh->setVertexArray(Mesh::Texcoord0, Mesh::TypeFloat, 2, uv);
    finishMesh(mesh, idx);
    return mesh;
}
// 0x080e25b0: unit icosahedron subdivided `segments` times (midpoints pushed onto the
// sphere), normals equal to the positions, spherical texcoords.
Mesh* createSphere(int segments)
{
    constexpr float phi = 1.618034f;               // golden ratio
    constexpr float icosahedronScale = 0.5257311f; // 1 / sqrt(1 + phi^2)
    constexpr int NumIcosahedronVertices = 12;
    constexpr int NumIcosahedronIndices = 60;
    static const Vec3 ico[NumIcosahedronVertices] = {
        Vec3(phi, 1, 0), Vec3(-phi, 1, 0), Vec3(phi, -1, 0), Vec3(-phi, -1, 0),
        Vec3(1, 0, phi), Vec3(1, 0, -phi), Vec3(-1, 0, phi), Vec3(-1, 0, -phi),
        Vec3(0, phi, 1), Vec3(0, -phi, 1), Vec3(0, phi, -1), Vec3(0, -phi, -1)};
    // 0x0823a720
    static constexpr int icoIdx[NumIcosahedronIndices] = {
        0, 8, 4, 0,  5, 10, 2, 4, 9, 2,  11, 5, 1,  6, 8,  1, 10, 7,  3, 9,
        6, 3, 7, 11, 0, 10, 8, 1, 8, 10, 2,  9, 11, 3, 11, 9, 4,  2,  0, 5,
        0, 2, 6, 1,  3, 7,  3, 1, 8, 6,  4,  9, 4,  6, 10, 5, 7,  11, 7, 5};
    Array<Vec3> pos;
    for (int i = 0; i < NumIcosahedronVertices; ++i)
        pos.push_back(normalize(ico[i] * icosahedronScale));
    Array<int> idx;
    for (int i = 0; i < NumIcosahedronIndices; ++i)
        idx.push_back(icoIdx[i]);
    for (int level = 0; level < segments; ++level)
    {
        // one new vertex per edge, keyed by (min << 16) + max
        core::HashMap<unsigned int, int> edges;
        struct Edge
        {
            int a, b, mid;
        };
        Array<Edge> edgeList;
        int numTris = idx.size() / 3;
        for (int t = 0; t < numTris; ++t)
        {
            for (int k = 0; k < 3; ++k)
            {
                int a = idx[t * 3 + k], b = idx[t * 3 + (k + 1) % 3];
                unsigned int key = a < b ? ((unsigned)a << 16) + b : ((unsigned)b << 16) + a;
                if (edges.findValue(key))
                    continue;
                edges.insert(key, edgeList.size());
                Edge edge = {a < b ? a : b, a < b ? b : a, -1};
                edgeList.push_back(edge);
            }
        }
        for (int i = 0; i < edgeList.size(); ++i)
        {
            Vec3 mid = (pos[edgeList[i].a] + pos[edgeList[i].b]) * 0.5f;
            edgeList[i].mid = pos.size();
            pos.push_back(normalize(mid));
        }
        for (int t = 0; t < numTris; ++t)
        {
            int a = idx[t * 3], b = idx[t * 3 + 1], c = idx[t * 3 + 2];
            int ab = edgeList[*edges.findValue(a < b ? ((unsigned)a << 16) + b
                                                     : ((unsigned)b << 16) + a)]
                         .mid;
            int bc = edgeList[*edges.findValue(b < c ? ((unsigned)b << 16) + c
                                                     : ((unsigned)c << 16) + b)]
                         .mid;
            int ca = edgeList[*edges.findValue(c < a ? ((unsigned)c << 16) + a
                                                     : ((unsigned)a << 16) + c)]
                         .mid;
            idx[t * 3] = ab;
            idx[t * 3 + 1] = bc;
            idx[t * 3 + 2] = ca;
            idx.push_back(a);
            idx.push_back(ab);
            idx.push_back(ca);
            idx.push_back(b);
            idx.push_back(bc);
            idx.push_back(ab);
            idx.push_back(c);
            idx.push_back(ca);
            idx.push_back(bc);
        }
    }
    Array<Vec2> uv;
    for (int i = 0; i < pos.size(); ++i)
    {
        const Vec3& p = pos[i];
        float lon = std::atan(p.x / p.z) + (p.z <= 0.0f ? -HALF_PI : HALF_PI);
        float lat = std::atan(p.y / std::sqrt(p.z * p.z + p.x * p.x));
        uv.push_back(Vec2(-(lon / TWO_PI + 0.75f), -lat / PI + 0.5f));
    }
    Mesh* mesh = new Mesh(pos.size());
    mesh->setVertexArray(Mesh::Position, Mesh::TypeFloat, 3, pos.data());
    mesh->setVertexArray(Mesh::Normal, Mesh::TypeFloat, 3, pos.data());
    mesh->setVertexArray(Mesh::Texcoord0, Mesh::TypeFloat, 2, uv.data());
    finishMesh(mesh, idx);
    return mesh;
}
// 0x080e2040: unit ring at z = 1 closed by a fan, apex at the origin.
Mesh* createCone(int segments)
{
    Array<Vec3> pos;
    for (int i = 0; i < segments; ++i)
    {
        float a = ((float)i * PI + (float)i * PI) / (float)segments;
        pos.push_back(Vec3(std::cos(a), std::sin(a), 1.0f));
    }
    pos.push_back(Vec3(0, 0, 0));
    int apex = pos.size() - 1;
    Array<int> idx;
    for (int i = 0; i < segments; ++i)
    {
        idx.push_back(i);
        idx.push_back((i + 1) % apex);
        idx.push_back(apex);
    }
    for (int i = 0; i < segments - 2; ++i)
    {
        idx.push_back(0);
        idx.push_back(i + 2);
        idx.push_back(i + 1);
    }
    Mesh* mesh = new Mesh(pos.size());
    mesh->setVertexArray(Mesh::Position, Mesh::TypeFloat, 3, pos.data());
    finishMesh(mesh, idx);
    mesh->computeVertexNormals();
    return mesh;
}
// 0x080e3950: quad on the xz plane.
Mesh* createPlane(float x0, float z0, float x1, float z1)
{
    Mesh* mesh = new Mesh(4);
    Vec3 pos[4] = {Vec3(x0, 0, z0), Vec3(x1, 0, z0), Vec3(x1, 0, z1), Vec3(x0, 0, z1)};
    Vec2 uv[4] = {Vec2(0, 0), Vec2(1, 0), Vec2(1, 1), Vec2(0, 1)};
    mesh->setVertexArray(Mesh::Position, Mesh::TypeFloat, 3, pos);
    mesh->setVertexArray(Mesh::Texcoord0, Mesh::TypeFloat, 2, uv);
    Array<int> idx;
    static constexpr int order[6] = {0, 2, 1, 0, 3, 2};
    for (int i = 0; i < 6; ++i)
        idx.push_back(order[i]);
    finishMesh(mesh, idx);
    mesh->computeVertexNormals();
    return mesh;
}
// 0x080e6a30: 4 faces x 3 unshared vertices of a unit-ish regular tetrahedron.
Mesh* createTetrahedron()
{
    static const Vec3 A(0.5773503f, -0.2041242f, 0.0f), B(-0.2886751f, -0.2041242f, 0.5f),
        C(-0.2886751f, -0.2041242f, -0.5f), D(0.0f, 0.6123725f, 0.0f);
    Vec3 pos[12] = {A, B, C, B, D, C, C, D, A, D, B, A};
    Vec2 uv[12];
    int idx[12];
    for (int i = 0; i < 12; ++i)
    {
        uv[i] = i % 3 == 0 ? Vec2(0, 0) : (i % 3 == 1 ? Vec2(1, 0) : Vec2(0, 1));
        idx[i] = i;
    }
    Mesh* mesh = new Mesh(12);
    mesh->setVertexArray(Mesh::Position, Mesh::TypeFloat, 3, pos);
    mesh->setVertexArray(Mesh::Texcoord0, Mesh::TypeFloat, 2, uv);
    Array<int> indices;
    for (int i = 0; i < 12; ++i)
        indices.push_back(idx[i]);
    finishMesh(mesh, indices);
    mesh->computeVertexNormals();
    return mesh;
}

} // namespace engine
