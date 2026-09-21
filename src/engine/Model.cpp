// Reconstructed from Grimrock.bin.x86 Model.cpp.
#include "engine/Model.h"
#include "core/Exception.h"
#include "core/FileStream.h"
#include "core/FileSystem.h"
#include "core/Sys.h"
#include "engine/Mesh.h"
#include "engine/Node.h"
#include "engine/RenderEntity.h"
#include "engine/Scene.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace engine
{

using namespace core;

constexpr unsigned int ModelFormatTag = 0x314c444d; // "MDL1"
constexpr unsigned int ModelFormatVersion = 2;
constexpr int ObjImportReserve = 0x2000; // initial vertex/index capacity of the .obj reader

Array<Model*> Model::sm_models;

// 0x080fbc70
Model::Model() : m_pRoot(0)
{
    sm_models.push_back(this);
}
// 0x080fe2e0
Model::~Model()
{
    for (int i = 0; i < m_nodes.size(); ++i)
        delete m_nodes[i];
    sm_models.remove(this);
}
// 0x080fbd60
Node* Model::createNode()
{
    Node* node = new Node;
    if (!m_pRoot)
        m_pRoot = node;
    else
        node->addTo(m_pRoot);
    m_nodes.push_back(node);
    return node;
}
// 0x080fbb40
int Model::getNodeIndex(const Node* node) const
{
    if (!node)
        return -1;
    for (int i = 0; i < m_nodes.size(); ++i)
        if (m_nodes[i] == node)
            return i;
    return -1;
}
// 0x080fbc00
Model* Model::getModelByFilename(const char* filename)
{
    for (int i = 0; i < sm_models.size(); ++i)
        if (strcmp(sm_models[i]->m_filename.c_str(), filename) == 0)
            return sm_models[i];
    return 0;
}

// 0x080fdc90
Node* Model::instantiate(Scene& scene) const
{
    Array<Node*> nodes;
    for (int i = 0; i < m_nodes.size(); ++i)
        nodes.push_back(scene.addNode());
    for (int i = 0; i < m_nodes.size(); ++i)
    {
        const Node* src = m_nodes[i];
        Node* node = nodes[i];
        node->setName(src->getName().c_str());
        node->setLocalMatrix(src->getLocalMatrix());
        int parent = getNodeIndex(src->getParent());
        if (parent >= 0)
            node->addTo(nodes[parent]);
        if (src->getMeshEntity())
        {
            MeshEntity* entity = new MeshEntity(*src->getMeshEntity());
            node->setRenderEntity(entity);
            const Skeleton* srcSkeleton = src->getMeshEntity()->getSkeleton();
            if (srcSkeleton)
            {
                Skeleton* skeleton = new Skeleton;
                for (int b = 0; b < srcSkeleton->getBoneCount(); ++b)
                {
                    int bone = getNodeIndex(srcSkeleton->getBone(b));
                    skeleton->addBone(*nodes[bone], srcSkeleton->getInvBindMatrix(b));
                }
                entity->setSkeleton(skeleton);
            }
        }
        else if (src->getLightEntity())
        {
            node->setRenderEntity(new LightEntity(*src->getLightEntity()));
        }
    }
    return nodes.size() > 0 ? nodes[0] : 0;
}

// 0x080fbe50
ModelAssetProcessor::ModelAssetProcessor()
{
    setExtensions(ModelAsset, "", "model");
}
ModelAssetProcessor::~ModelAssetProcessor() {}

// 0x080fbed0: positions and triangle faces only.
static Model* objLoader(const char* filename)
{
    int length;
    char* text = readFile(filename, length);
    Array<Vec3> positions;
    Array<int> indices;
    positions.reserve(ObjImportReserve);
    indices.reserve(ObjImportReserve);
    for (char* line = strtok(text, "\n"); line; line = strtok(0, "\n"))
    {
        if (line[0] == 'v' && (line[1] == ' ' || line[1] == '\t'))
        {
            Vec3 position(0, 0, 0);
            sscanf(line, "v %f %f %f", &position.x, &position.y, &position.z);
            positions.push_back(position);
        }
        else if (line[0] == 'f')
        {
            // f v[/vt[/vn]] ... ; only the vertex index of each corner is used
            int corners[4] = {-1, -1, -1, -1};
            int numCorners = 0;
            char* p = line + 1;
            while (*p && numCorners < 4)
            {
                while (*p == ' ' || *p == '\t')
                    ++p;
                if (!*p)
                    break;
                corners[numCorners++] = (int)strtol(p, &p, 10) - 1;
                while (*p && *p != ' ' && *p != '\t')
                    ++p;
            }
            if (numCorners < 3)
                continue;
            for (int c = 0; c < 3; ++c)
                if (corners[c] < 0 || corners[c] >= positions.size())
                    throw Exception("Malformed obj file: %s", filename);
            indices.push_back(corners[0]);
            indices.push_back(corners[1]);
            indices.push_back(corners[2]);
        }
    }
    delete[] text;
    Mesh* mesh = new Mesh(positions.size());
    mesh->setVertexArray(Mesh::Position, Mesh::TypeFloat, 3, positions.data());
    mesh->setIndices(indices.data(), indices.size());
    MeshSegment& segment = mesh->addSegment();
    segment.primitiveType = 2;
    segment.firstIndex = 0;
    segment.numTriangles = indices.size() / 3;
    mesh->computeVertexNormals();
    Model* model = new Model;
    Node* node = model->createNode();
    node->setRenderEntity(new MeshEntity(createRenderableMesh(*mesh, true)));
    return model;
}

// 0x080fe410
void ModelAssetProcessor::processSingleFile(const char* source, const char* native)
{
    String ext = core::getFileExtension(source);
    if (ext != "obj")
        throw Exception("Unknown file format: %s", source);
    Model* model = objLoader(source);
    FileOutputStream out(native);
    out.writeInt((int)ModelFormatTag);
    out.writeInt((int)ModelFormatVersion);
    out.writeInt(model->m_nodes.size());
    for (int i = 0; i < model->m_nodes.size(); ++i)
    {
        Node* node = model->m_nodes[i];
        out.writeString(node->getName().c_str());
        out.writeMatrix4x3(node->getLocalMatrix());
        out.writeInt(node->getParent() ? model->getNodeIndex(node->getParent()) : -1);
        RenderEntity* entity = node->getRenderEntity();
        if (!entity)
        {
            out.writeInt(-1);
            continue;
        }
        out.writeInt(entity->getEntityType());
        if (entity->getEntityType() == RenderEntity::MeshEntityType)
        {
            MeshEntity* meshEntity = node->getMeshEntity();
            Mesh* mesh = meshEntity->getMesh()->getSourceData();
            if (!mesh)
                throw Exception("No source data");
            saveMesh(*mesh, out);
            const Skeleton* skeleton = meshEntity->getSkeleton();
            if (!skeleton)
            {
                out.writeInt(0);
            }
            else
            {
                out.writeInt(skeleton->getBoneCount());
                for (int b = 0; b < skeleton->getBoneCount(); ++b)
                {
                    out.writeInt(model->getNodeIndex(skeleton->getBone(b)));
                    out.writeMatrix4x3(skeleton->getInvBindMatrix(b));
                }
            }
            out.writeVector3(meshEntity->getEmissiveColor());
            out.writeBool((meshEntity->getFlags() & MeshEntity::CastShadow) != 0);
        }
        else if (entity->getEntityType() == RenderEntity::LightEntityType)
        {
            LightEntity* lightEntity = node->getLightEntity();
            out.writeInt(lightEntity->getLightType());
            out.writeVector3(lightEntity->getLightColor());
            out.writeFloat(lightEntity->getLightRange());
            out.writeFloat(lightEntity->getSpotAngle());
            out.writeFloat(lightEntity->getSpotSharpness());
            out.writeBool(lightEntity->getCastShadow());
        }
    }
    delete model;
}

static void checkHeader(InputStream& in)
{
    int tag, version;
    in.readInt(tag);
    if (tag != (int)ModelFormatTag)
    {
        throw Exception("Invalid file format: %s", in.getFilename());
    }
    in.readInt(version);
    if (version != ModelFormatVersion)
        throw Exception("Invalid file version (expected v%d, got v%d): %s", ModelFormatVersion,
                        version, in.getFilename());
}

// 0x080fc6e0
Model* loadModel(const char* filename, bool keepSourceData)
{
    Model* cached = Model::getModelByFilename(filename);
    if (cached)
        return cached;
    AssetProcessor* processor = findAssetProcessor(AssetProcessor::ModelAsset, filename);
    processor->processFile(filename);
    String native = processor->getNativeFile(filename);
    FileInputStream in(native.c_str());
    Model* model = new Model;
    checkHeader(in);
    int numNodes;
    in.readInt(numNodes);
    for (int i = 0; i < numNodes; ++i)
        model->createNode();
    for (int i = 0; i < numNodes; ++i)
    {
        Node* node = model->m_nodes[i];
        String name;
        in.readString(name);
        node->setName(name.c_str());
        Matrix4x3 local;
        in.readMatrix4x3(local);
        node->setLocalMatrix(local);
        int parent;
        in.readInt(parent);
        if (parent >= 0)
            node->addTo(model->m_nodes[parent]);
        int entityType;
        in.readInt(entityType);
        if (entityType < 0)
            continue;
        if (entityType == RenderEntity::MeshEntityType)
        {
            MeshEntity* entity = new MeshEntity(0);
            Mesh* mesh = loadMesh(in);
            entity->setMesh(createRenderableMesh(*mesh, keepSourceData));
            int numBones;
            in.readInt(numBones);
            if (numBones > 0)
            {
                Skeleton* skeleton = new Skeleton;
                for (int b = 0; b < numBones; ++b)
                {
                    int boneNode;
                    in.readInt(boneNode);
                    Matrix4x3 invBind;
                    in.readMatrix4x3(invBind);
                    skeleton->addBone(*model->m_nodes[boneNode], invBind);
                }
                entity->setSkeleton(skeleton);
            }
            Vec3 emissive;
            in.readVector3(emissive);
            entity->setEmissiveColor(emissive);
            bool castShadow;
            in.readBool(castShadow);
            entity->setFlag(MeshEntity::CastShadow, castShadow);
            node->setRenderEntity(entity);
            delete mesh;
        }
        else if (entityType == RenderEntity::LightEntityType)
        {
            int lightType;
            in.readInt(lightType);
            LightEntity* entity = new LightEntity((LightEntity::LightType)lightType);
            Vec3 color;
            in.readVector3(color);
            entity->setLightColor(color);
            float value;
            in.readFloat(value);
            entity->setLightRange(value);
            in.readFloat(value);
            entity->setSpotAngle(value);
            in.readFloat(value);
            entity->setSpotSharpness(value);
            bool castShadow;
            in.readBool(castShadow);
            entity->setCastShadow(castShadow);
            node->setRenderEntity(entity);
        }
        else
        {
            throw Exception("Invalid entity type");
        }
    }
    model->m_filename = filename;
    return model;
}

// 0x080fd0a0: reads the node hierarchy, transforms every mesh into world space and
// merges them into one mesh.
Mesh* meshLoader(InputStream& in)
{
    checkHeader(in);
    int numNodes;
    in.readInt(numNodes);
    Array<Matrix4x3> localMatrices;
    localMatrices.resize(numNodes, Matrix4x3());
    Array<int> parents;
    Array<Mesh*> meshes;
    parents.resize(numNodes, -1);
    meshes.resize(numNodes, (Mesh*)0);
    for (int i = 0; i < numNodes; ++i)
    {
        String name;
        in.readString(name);
        in.readMatrix4x3(localMatrices[i]);
        in.readInt(parents[i]);
        int entityType;
        in.readInt(entityType);
        if (entityType < 0)
            continue;
        if (entityType == RenderEntity::MeshEntityType)
        {
            meshes[i] = loadMesh(in);
            int numBones;
            in.readInt(numBones);
            for (int b = 0; b < numBones; ++b)
            {
                int boneNode;
                in.readInt(boneNode);
                Matrix4x3 invBind;
                in.readMatrix4x3(invBind);
            }
            Vec3 emissive;
            in.readVector3(emissive);
        }
        else if (entityType == RenderEntity::LightEntityType)
        {
            int lightType;
            in.readInt(lightType);
            Vec3 color;
            in.readVector3(color);
            float range, spotAngle, spotSharpness; // read and discarded like the original
            in.readFloat(range);
            in.readFloat(spotAngle);
            in.readFloat(spotSharpness);
        }
        else
        {
            throw Exception("Invalid entity type");
        }
        bool flag;
        in.readBool(flag);
    }
    for (int i = 0; i < numNodes; ++i)
    {
        if (!meshes[i])
            continue;
        Matrix4x3 m = localMatrices[i];
        for (int p = parents[i]; p >= 0; p = parents[p])
            m = localMatrices[p] * m;
        meshes[i]->transform(m);
    }
    Array<Mesh*> valid;
    for (int i = 0; i < numNodes; ++i)
        if (meshes[i])
            valid.push_back(meshes[i]);
    Mesh* result;
    if (valid.size() == 0)
    {
        result = 0;
    }
    else if (valid.size() == 1)
    {
        result = valid[0];
    }
    else
    {
        debugPrint("WARNING! merging meshes: %s\n", in.getFilename());
        result = Mesh::mergeMeshes(valid, 0);
        for (int i = 0; i < valid.size(); ++i)
            delete valid[i];
    }
    return result;
}

} // namespace engine
