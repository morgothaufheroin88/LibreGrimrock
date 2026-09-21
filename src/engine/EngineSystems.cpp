// Reconstructed from Grimrock.bin.x86 EngineSystems.cpp.
#include "engine/EngineSystems.h"
#include "core/FileSystem.h"
#include "engine/AssetProcessor.h"

namespace engine
{

// 0x080eb660
EngineSystems::EngineSystems(const RendererConfig& config, int physicsEngine, int audioEngine)
{
    if (physicsEngine >= 0)
        m_physicsEngine.reset(PhysicsEngine::create(physicsEngine));
    if (audioEngine >= 0)
        m_audioEngine.reset(AudioEngine::create(audioEngine));
    if (config.altShaderPath.size() > 0)
        core::addSearchPath(config.altShaderPath.c_str());
    m_renderer.reset(Renderer::create(config.renderEngine));
    m_renderer->init(config);
    registerAssetProcessor(AssetProcessor::ModelAsset, "fbx", "model");
    registerAssetProcessor(AssetProcessor::ModelAsset, "obj", "model");
    registerAssetProcessor(AssetProcessor::ModelAsset, "lwo", "model");
    registerAssetProcessor(AssetProcessor::AnimationAsset, "fbx", "animation");
}
// 0x080eb7d0
EngineSystems::~EngineSystems() {}

} // namespace engine
