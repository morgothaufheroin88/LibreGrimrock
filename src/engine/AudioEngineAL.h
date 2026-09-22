// OpenAL audio engine with threaded Ogg Vorbis streaming, reconstructed from
// AudioEngineAL.cpp (0x080f4d60-0x080f7470).
#pragma once
#include "core/Array.h"
#include "core/SharedPtr.h"
#include "core/Thread.h"
#include "engine/AudioEngine.h"
#include <AL/al.h>
#include <AL/alc.h>
#include <vorbis/vorbisfile.h>

namespace engine
{

class AudioWorldAL;
class AudioListener;

class SampleAL : public Sample
{
  public:
    SampleAL();
    ~SampleAL();
    // 0x080f55c0: 8/16 bit mono/stereo PCM WAVE.
    void load(const char* filename);
    ALuint getBuffer() const
    {
        return m_buffer;
    }

  private:
    ALuint m_buffer;
};

// Fills free buffers with decoded audio and queues them on the source (0x080f5c50).
class OggDecodingThreadAL : public core::Thread
{
  public:
    static constexpr int DecodeBufferSize = 0x20000;
    OggDecodingThreadAL();
    ~OggDecodingThreadAL();
    void run();

    core::Array<ALuint> m_freeBuffers; // 0 entry = stop request
    core::ScopedPtr<core::Mutex> m_mutex;
    OggVorbis_File m_file;
    ALuint m_source;
    core::ScopedPtr<core::SyncEvent> m_fillRequest;
    core::ScopedPtr<core::SyncEvent> m_finished;
    bool m_loop;
    char* m_pDecodeBuffer;
    bool m_done;
};

class AudioStreamAL
{
  public:
    AudioStreamAL(ALuint source, const char* filename, bool loop);
    ~AudioStreamAL();
    void play() {}
    void pause() {}
    void stop() {}
    bool isPlaying();
    // Recycles the processed buffers.
    void update();
    OggDecodingThreadAL* getThread() const
    {
        return m_pThread;
    }

  private:
    OggDecodingThreadAL* m_pThread;
    ALuint m_buffers[3];
};

class SoundSourceAL : public SoundSource
{
  public:
    // state at +0x14
    enum State
    {
        Stopped = 0,
        Starting = 1,
        Playing = 2,
        Suspended = 3
    };
    explicit SoundSourceAL(AudioWorldAL* world);
    ~SoundSourceAL();
    void play(Sample& sample, bool positional);
#if GRIMROCK_GAME >= 2
    void play(Sample& sample, bool positional, int startSample);
    void setPitch(float pitch);
    float getPitch() const
    {
        return m_pitch;
    }
    PlayState getPlayState() const;
#endif
    void playStream(const char* filename);
    void stop();
    bool isPlaying() const
    {
        return m_state > Stopped;
    }
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
    int getSamplePosition() const;
    bool getLoop() const
    {
        return m_loop;
    }

    void start(AudioListener& listener);
    void suspend();
    void update(AudioListener& listener);
    void updateVolume(AudioListener& listener);
    State getState() const
    {
        return m_state;
    }
    void detachWorld()
    {
        m_pWorld = 0;
    }

  private:
    State m_state;
    ALuint m_source;
    AudioWorldAL* m_pWorld;
    AudioStreamAL* m_pStream;
    bool m_positional;
    float m_volume;
    bool m_mute;
    float m_minDistance;
    float m_maxDistance;
    bool m_loop;
#if GRIMROCK_GAME >= 2
    float m_pitch;
    int m_startSample;
    bool m_finished; // the last sample ran to its end (not stopped)
#endif
};

class AudioWorldAL : public AudioWorld
{
  public:
    AudioWorldAL();
    ~AudioWorldAL();
    SoundSource* createSoundSource();
    void setVolume(float volume)
    {
        m_volume = volume;
    }
    float getVolume() const
    {
        return m_volume;
    }
    void soundSourceDeleted(SoundSourceAL* source);

    core::Array<SoundSourceAL*> m_sources;
    bool m_rendered;
    float m_volume;
};

class AudioListenerAL : public AudioListener
{
  public:
    AudioListenerAL() : m_volume(1.0f) {}
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

class AudioEngineAL : public AudioEngine
{
  public:
    AudioEngineAL();
    ~AudioEngineAL();
    AudioWorld* createWorld();
    AudioListener* createListener();
    Sample* createSample();
    void beginFrame();
    // Starts and updates the sources of the world, worlds not rendered in a frame are
    // paused by endFrame.
    void render(AudioWorld& world, AudioListener& listener);
    void endFrame();
    void worldDeleted(AudioWorldAL* world);

  private:
    ALCdevice* m_pDevice;
    ALCcontext* m_pContext;
    core::Array<AudioWorldAL*> m_worlds;
};

} // namespace engine
