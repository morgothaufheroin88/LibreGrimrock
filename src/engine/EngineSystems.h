// Owns the renderer, physics and audio engines, reconstructed from EngineSystems.cpp
// (0x080eb620-0x080eb7d0).
#pragma once
#include "core/SharedPtr.h"
#include "engine/AudioEngine.h"
#include "engine/PhysicsEngine.h"
#include "engine/Renderer.h"

namespace engine
{

class EngineSystems
{
  public:
    // physicsEngine / audioEngine < 0 leaves that system uninitialised.
    EngineSystems(const RendererConfig& config, int physicsEngine, int audioEngine);
    ~EngineSystems();
    void configureAssetPipeline(int flags) {}
    Renderer* getRenderer() const
    {
        return m_renderer.get();
    }
    PhysicsEngine* getPhysicsEngine() const
    {
        return m_physicsEngine.get();
    }
    AudioEngine* getAudioEngine() const
    {
        return m_audioEngine.get();
    }

  private:
    core::SharedPtr<Renderer> m_renderer;
    core::SharedPtr<PhysicsEngine> m_physicsEngine;
    core::SharedPtr<AudioEngine> m_audioEngine;
};

} // namespace engine
