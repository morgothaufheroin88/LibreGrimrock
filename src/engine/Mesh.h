// Mesh data and renderable meshes, reconstructed from Mesh.cpp (0x080dd480-0x080eb3b0).
// A Mesh keeps 15 raw vertex arrays (position, normal, tangent, bitangent, color,
// texcoord0..7, boneIndices, boneWeights), an index list and material segments.
#pragma once
#include "core/Array.h"
#include "core/Prim.h"
#include "core/SharedPtr.h"
#include "core/Stream.h"
#include "core/String.h"

namespace engine
{

class Material;

struct MeshSegment
{
    core::SharedPtr<Material> material;
    int primitiveType; // Mesh::PrimitiveType
    int firstIndex;
    int numTriangles;
};

class Mesh
{
  public:
    enum VertexArrayIndex
    {
        Position = 0,
        Normal = 1,
        Tangent = 2,
        Bitangent = 3,
        Color = 4,
        Texcoord0 = 5,
        BoneIndices = 13,
        BoneWeights = 14,
        NumVertexArrays = 15
    };
    // Vertex array element types (0x080de520 table).
    enum VertexType
    {
        TypeByte = 0,
        TypeShort = 1,
        TypeInt = 2,
        TypeFloat = 3
    };
    // MeshSegment::primitiveType, the order of Mesh.addSegment's names; a primitive has
    // primitiveType + 1 indices
    enum PrimitiveType
    {
        PointList = 0,
        LineList = 1,
        TriangleList = 2,
        QuadList = 3
    };

    struct VertexArray
    {
        void* pData;
        int type;
        int components;
        int stride;
    };
    struct SortVertex;

    explicit Mesh(int numVertices = 0);
    Mesh(const Mesh& other);
    virtual ~Mesh();

    const core::String& getFilename() const
    {
        return m_filename;
    }
    void setFilename(const char* name)
    {
        m_filename = name;
    }
    int getNumVertices() const
    {
        return m_numVertices;
    }
    void setNumVertices(int n);
    // 0x080dd640
    const void* getVertexArray(int index, int = 0, int = 0) const
    {
        return m_vertexArrays[index].pData;
    }
    void* getVertexArray(int index)
    {
        return m_vertexArrays[index].pData;
    }
    const VertexArray& getVertexArrayInfo(int index) const
    {
        return m_vertexArrays[index];
    }
    // 0x080de520: copies numVertices * stride bytes; data may be null to clear.
    void setVertexArray(int index, int type, int components, const void* data);
    static int getVertexTypeSize(int type);

    const core::Array<int>& getIndices() const
    {
        return m_indices;
    }
    core::Array<int>& getIndices()
    {
        return m_indices;
    }
    void setIndices(const int* indices, int count);
    MeshSegment& addSegment();
    void clearSegments();
    int getNumSegments() const
    {
        return m_segments.size();
    }
    MeshSegment& getSegment(int i)
    {
        return m_segments[i];
    }
    const MeshSegment& getSegment(int i) const
    {
        return m_segments[i];
    }
    core::Array<MeshSegment>& getSegments()
    {
        return m_segments;
    }
    const core::AABox3& getBoundingBox() const
    {
        return m_bounds;
    }
    const core::Vec3& getBoundingSphereCenter() const
    {
        return m_boundingSphereCenter;
    }
    float getBoundingSphereRadius() const
    {
        return m_boundingSphereRadius;
    }
    void setBounds(const core::AABox3& box, const core::Vec3& center, float radius)
    {
        m_bounds = box;
        m_boundingSphereCenter = center;
        m_boundingSphereRadius = radius;
    }

    void updateBounds();
    void computeVertexNormals();
    void computeTangentVectors();
    void normalizeBoneWeights();
    void flipFaces();
    void triangulate();
    void scale(float s);
    void translate(const core::Vec3& t);
    void transform(const core::Matrix4x3& m);
    void rotate(const core::Matrix3x3& m);
    bool raycast(const core::Ray3& ray, float& t, int* triangle, core::Vec3* normal);
    bool raycast(const core::Ray3& ray);
    static int compareVertex(const Mesh& mesh, int a, int b);
    void weldVertices();
    void optimizeSegments();
#if GRIMROCK_GAME >= 2
    // 0x004abaf0: every index gets its own vertex again (the reverse of weldVertices)
    void unweldVertices();
    // 0x004adb00: the material of every segment
    void setMaterial(Material* material);
#endif
    static bool sortByMaterial(const MeshSegment& a, const MeshSegment& b);
    static Mesh* mergeMeshes(const core::Array<Mesh*>& meshes,
                             const core::Array<core::Matrix4x3>* transforms);
    static Mesh* getMeshByFilename(const char* filename);

  private:
    core::String m_filename;
    int m_numVertices;
    VertexArray m_vertexArrays[NumVertexArrays];
    core::Array<int> m_indices;
    core::Array<MeshSegment> m_segments;
    core::Vec3 m_boundingSphereCenter;
    float m_boundingSphereRadius;
    core::AABox3 m_bounds;
    static core::Array<Mesh*> sm_meshes;
};

class RenderableMesh
{
  public:
    RenderableMesh();
    virtual ~RenderableMesh();
    virtual void init(Mesh& mesh, bool keepSourceData) = 0;
    virtual Mesh* getSourceData() = 0;
    virtual const core::AABox3& getBoundingBox() const = 0;
    virtual core::Array<core::SharedPtr<Material>> getMaterials() const = 0;

    const core::String& getFilename() const
    {
        return m_filename;
    }
    void setFilename(const char* name)
    {
        m_filename = name;
    }
    static RenderableMesh* getRenderableMeshByFilename(const char* filename);

  protected:
    core::String m_filename;
    static core::Array<RenderableMesh*> sm_meshes;
};

// Procedural meshes (0x080e2040-0x080e6e40).
Mesh* createBox(float width, float height, float depth);
Mesh* createSphere(int segments);
Mesh* createCone(int segments);
Mesh* createPlane(float x0, float z0, float x1, float z1);
Mesh* createTetrahedron();

// "MESH" version 2 files.
Mesh* loadMesh(core::InputStream& stream);
Mesh* loadMesh(const char* filename); // cached by filename, via the model asset processor
void saveMesh(Mesh& mesh, core::OutputStream& stream);
RenderableMesh* createRenderableMesh(Mesh& mesh, bool keepSourceData);
RenderableMesh* loadRenderableMesh(const char* filename, bool keepSourceData);

} // namespace engine
