// Node hierarchies with mesh and light entities, reconstructed from Model.cpp
// (0x080fbb40-0x080fee80). Native format "MDL1" version 2.
#pragma once
#include "core/Array.h"
#include "core/String.h"
#include "engine/AssetProcessor.h"

namespace core
{
class InputStream;
}

namespace engine
{

class Node;
class Scene;
class Mesh;

class Model
{
  public:
    Model();
    ~Model();
    // Appends a node under the root (the first node becomes the root).
    Node* createNode();
    int getNodeIndex(const Node* node) const;
    // 0x080fdc90: copies the hierarchy into the scene, returns the new root.
    Node* instantiate(Scene& scene) const;
    const core::String& getFilename() const
    {
        return m_filename;
    }
    Node* getRootNode() const
    {
        return m_pRoot;
    }
    const core::Array<Node*>& getNodes() const
    {
        return m_nodes;
    }
    static Model* getModelByFilename(const char* filename);
    static core::Array<Model*> sm_models;

  private:
    friend Model* loadModel(const char* filename, bool keepSourceData);
    friend class ModelAssetProcessor;
    core::String m_filename;
    Node* m_pRoot;
    core::Array<Node*> m_nodes;
};

// Converts .obj files to the native model format.
class ModelAssetProcessor : public SingleFileAssetProcessor
{
  public:
    ModelAssetProcessor();
    ~ModelAssetProcessor();
    void processSingleFile(const char* source, const char* native);
};

// 0x080fc6e0: cached by filename, goes through the model asset processor.
Model* loadModel(const char* filename, bool keepSourceData);
// 0x080fd0a0: all meshes of a model file merged into world space.
Mesh* meshLoader(core::InputStream& stream);

} // namespace engine
