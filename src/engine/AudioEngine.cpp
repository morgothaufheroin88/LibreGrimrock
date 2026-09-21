// Reconstructed from Grimrock.bin.x86 AudioEngine.cpp.
#include "engine/AudioEngine.h"
#include "core/Exception.h"
#include "engine/AudioEngineAL.h"
#include <cstring>

namespace engine
{

using namespace core;

Array<Sample*> Sample::sm_samples;
AudioEngine* AudioEngine::sm_pActiveAudioEngine = 0;

// 0x080f3ed0
Sample::Sample()
{
    sm_samples.push_back(this);
}
// 0x080f3db0
Sample::~Sample()
{
    sm_samples.remove(this);
}
// 0x080f4220
void Sample::reload()
{
    load(m_filename.c_str());
}
// 0x080f3e60
Sample* Sample::getSampleByFilename(const char* filename)
{
    for (int i = 0; i < sm_samples.size(); ++i)
        if (strcmp(sm_samples[i]->m_filename.c_str(), filename) == 0)
            return sm_samples[i];
    return 0;
}

// 0x080f3fb0
AudioEngine* AudioEngine::create(int engine)
{
    if (engine == Engine_Null)
    {
        sm_pActiveAudioEngine = new AudioEngineNull;
        return sm_pActiveAudioEngine;
    }
    if (engine != Engine_OpenAL)
        throw Exception("Invalid audio engine");
    sm_pActiveAudioEngine = new AudioEngineAL;
    return sm_pActiveAudioEngine;
}

// 0x080f4080
Sample* loadSample(const char* filename)
{
    Sample* cached = Sample::getSampleByFilename(filename);
    if (cached)
        return cached;
    Sample* sample = AudioEngine::sm_pActiveAudioEngine->createSample();
    sample->load(filename);
    sample->m_filename = filename;
    return sample;
}

// 0x080daa50
void Node::setSoundSource(SoundSource* source)
{
    setComponent(Component::SoundSourceComponent, source);
}

} // namespace engine
