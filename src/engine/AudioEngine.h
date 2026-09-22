// Audio interface and the null engine, reconstructed from AudioEngine.cpp
// (0x080f3d40-0x080f4c80).
#pragma once
#include "core/Array.h"
#include "core/SharedPtr.h"
#include "core/String.h"
#include "engine/Node.h"

namespace engine
{

class Sample
{
  public:
    Sample();
    virtual ~Sample();
    virtual void load(const char* filename) = 0;
    // 0x080f4220
    virtual void reload();
    const core::String& getFilename() const
    {
        return m_filename;
    }
    static Sample* getSampleByFilename(const char* filename);
    static core::Array<Sample*> sm_samples;

  protected:
    friend Sample* loadSample(const char* filename);
    core::String m_filename;
};

class SoundSource : public Component
{
  public:
    static constexpr float DefaultMinDistance = 1.0f;
    static constexpr float DefaultMaxDistance = 10.0f;
    SoundSource() : Component(SoundSourceComponent) {}
    virtual ~SoundSource() {}
    virtual void play(Sample& sample, bool positional) = 0;
    virtual void playStream(const char* filename) = 0;
    virtual void stop() = 0;
    virtual bool isPlaying() const = 0;
    virtual void setVolume(float volume) = 0;
    virtual void setMute(bool mute) = 0;
    virtual void setMinDistance(float d) = 0;
    virtual void setMaxDistance(float d) = 0;
    virtual void setLoop(bool loop) = 0;
    virtual float getVolume() const = 0;
    virtual bool getMute() const = 0;
    virtual float getMinDistance() const = 0;
    virtual float getMaxDistance() const = 0;
    virtual int getSamplePosition() const = 0;
    virtual bool getLoop() const = 0;
#if GRIMROCK_GAME >= 2
    // Grimrock 2 (SoundSourceXA2 0x004c6540-0x004c7a00): playback from a sample offset,
    // pitch, and whether a sample ended by itself
    enum PlayState
    {
        Play_Playing = 0,
        Play_Stopped = 1,
        Play_Finished = 2
    };
    virtual void play(Sample& sample, bool positional, int startSample) = 0;
    virtual void setPitch(float pitch) = 0;
    virtual float getPitch() const = 0;
    virtual PlayState getPlayState() const = 0;
#endif

  protected:
    core::SharedPtr<Sample> m_sample;
};

class AudioWorld
{
  public:
    virtual ~AudioWorld() {}
    virtual SoundSource* createSoundSource() = 0;
    virtual void setVolume(float volume) = 0;
    virtual float getVolume() const = 0;
};

// The listener is a scene node.
class AudioListener : public Node
{
  public:
    virtual ~AudioListener() {}
    virtual void setVolume(float volume) = 0;
    virtual float getVolume() const = 0;
};

class AudioEngine
{
  public:
    enum Engine
    {
        Engine_Null = 0,
        Engine_FMOD = 1,
        Engine_OpenAL = 2,
        Engine_XAudio2 = 3
    };
    virtual ~AudioEngine() {}
    virtual AudioWorld* createWorld() = 0;
    virtual AudioListener* createListener() = 0;
    virtual Sample* createSample() = 0;
    virtual void beginFrame() = 0;
    virtual void render(AudioWorld& world, AudioListener& listener) = 0;
    virtual void endFrame() = 0;
    // 0x080f3fb0
    static AudioEngine* create(int engine);
    static AudioEngine* sm_pActiveAudioEngine;
};

// 0x080f4080: cached by filename.
Sample* loadSample(const char* filename);

// ---- null implementation -----------------------------------------------------------

class SampleNull : public Sample
{
  public:
    void load(const char* filename) {}
};

class SoundSourceNull : public SoundSource
{
  public:
    SoundSourceNull()
        : m_volume(1.0f), m_mute(false), m_minDistance(DefaultMinDistance),
          m_maxDistance(DefaultMaxDistance), m_loop(false)
    {
    }
    void play(Sample& sample, bool positional) {}
    void playStream(const char* filename) {}
    void stop() {}
    bool isPlaying() const
    {
        return false;
    }
#if GRIMROCK_GAME >= 2
    void play(Sample& sample, bool positional, int startSample) {}
    void setPitch(float pitch) {}
    float getPitch() const
    {
        return 1.0f;
    }
    PlayState getPlayState() const
    {
        return Play_Stopped;
    }
#endif
    void setVolume(float volume)
    {
        m_volume = volume;
    }
    void setMute(bool mute)
    {
        m_mute = mute;
    }
    void setMinDistance(float d)
    {
        m_minDistance = d;
    }
    void setMaxDistance(float d)
    {
        m_maxDistance = d;
    }
    void setLoop(bool loop)
    {
        m_loop = loop;
    }
    float getVolume() const
    {
        return m_volume;
    }
    bool getMute() const
    {
        return m_mute;
    }
    float getMinDistance() const
    {
        return m_minDistance;
    }
    float getMaxDistance() const
    {
        return m_maxDistance;
    }
    int getSamplePosition() const
    {
        return 0;
    }
    bool getLoop() const
    {
        return m_loop;
    }

  private:
    float m_volume;
    bool m_mute;
    float m_minDistance;
    float m_maxDistance;
    bool m_loop;
};

class AudioWorldNull : public AudioWorld
{
  public:
    AudioWorldNull() : m_volume(1.0f) {}
    SoundSource* createSoundSource()
    {
        return new SoundSourceNull;
    }
    void setVolume(float volume)
    {
        m_volume = volume;
    }
    float getVolume() const
    {
        return m_volume;
    }

  private:
    float m_volume;
};

class AudioListenerNull : public AudioListener
{
  public:
    AudioListenerNull() : m_volume(1.0f) {}
    void setVolume(float volume)
    {
        m_volume = volume;
    }
    float getVolume() const
    {
        return m_volume;
    }

  private:
    float m_volume;
};

class AudioEngineNull : public AudioEngine
{
  public:
    AudioWorld* createWorld()
    {
        return new AudioWorldNull;
    }
    AudioListener* createListener()
    {
        return new AudioListenerNull;
    }
    Sample* createSample()
    {
        return new SampleNull;
    }
    void beginFrame() {}
    void render(AudioWorld& world, AudioListener& listener) {}
    void endFrame() {}
};

} // namespace engine
