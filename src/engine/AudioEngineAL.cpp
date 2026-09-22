// Reconstructed from Grimrock.bin.x86 AudioEngineAL.cpp.
#include "engine/AudioEngineAL.h"
#include "core/Exception.h"
#include "core/FileSystem.h"
#include "core/Sys.h"
#include "engine/WavLoader.h"
#include <cmath>
#include <cstdlib>

namespace engine
{

using namespace core;

// Distance attenuation curve of positional sources (see SoundSourceAL::updateVolume).
constexpr float AttenuationSteepness = 16.0f;
constexpr float AttenuationOffset = 0.0588f;
constexpr float AttenuationScale = 1.0588f;
constexpr float AttenuationGainNear = 0.9965426f;
constexpr float AttenuationGainFar = 2.4912435e-05f;

static Exception alcError(ALCdevice* device, const char* where)
{
    Exception error;
    error.setReason("OpenAL context error in %s: %s", where,
                    alcGetString(device, alcGetError(device)));
    return error;
}
static Exception alError(const char* where)
{
    Exception error;
    error.setReason("OpenAL error in %s: %s", where, alGetString(alGetError()));
    return error;
}

// ---- vorbis callbacks over core::File --------------------------------------------

// 0x080f5250
static size_t vorbisReadFunc(void* ptr, size_t size, size_t nmemb, void* datasource)
{
    File* file = (File*)datasource;
    int pos = file->getFilePosition();
    int remaining = file->getFileLength() - pos;
    int bytes = (int)(size * nmemb);
    if (bytes > remaining)
        bytes = remaining;
    file->read(ptr, bytes);
    return file->getFilePosition() - pos;
}
// 0x080f51b0
static int vorbisSeekFunc(void* datasource, ogg_int64_t offset, int whence)
{
    File* file = (File*)datasource;
    if (whence == SEEK_SET)
        file->seek((int)offset);
    else if (whence == SEEK_CUR)
        file->skip((int)offset);
    else if (whence == SEEK_END)
        file->seek(file->getFileLength() - (int)offset);
    else
        return 1;
    return 0;
}
// 0x080f4f10
static long vorbisTellFunc(void* datasource)
{
    return ((File*)datasource)->getFilePosition();
}
// 0x080f5190
static int vorbisCloseFunc(void* datasource)
{
    closeFile((File*)datasource);
    return 0;
}

// ---- SampleAL --------------------------------------------------------------------

// 0x080f5730
SampleAL::SampleAL() : m_buffer(0)
{
    alGenBuffers(1, &m_buffer);
    if (!alIsBuffer(m_buffer))
        throw alError("alGenBuffers");
}
// 0x080f56e0
SampleAL::~SampleAL()
{
    alDeleteBuffers(1, &m_buffer);
}
// 0x080f55c0
void SampleAL::load(const char* filename)
{
    WavLoader wav;
    wav.loadSound(filename);
    ALenum format;
    if (wav.m_bitsPerSample == 8 && wav.m_channels == 1)
        format = AL_FORMAT_MONO8;
    else if (wav.m_bitsPerSample == 8 && wav.m_channels == 2)
        format = AL_FORMAT_STEREO8;
    else if (wav.m_bitsPerSample == 16 && wav.m_channels == 1)
        format = AL_FORMAT_MONO16;
    else if (wav.m_bitsPerSample == 16 && wav.m_channels == 2)
        format = AL_FORMAT_STEREO16;
    else
        throw Exception("OpenAL incompatible WAV file");
    alBufferData(m_buffer, format, wav.m_pData, wav.m_dataSize, wav.m_sampleRate);
}

// ---- OggDecodingThreadAL ---------------------------------------------------------

// 0x080f5c50
OggDecodingThreadAL::OggDecodingThreadAL()
    : m_mutex(new Mutex), m_source(0), m_fillRequest(new SyncEvent), m_finished(new SyncEvent),
      m_loop(false), m_pDecodeBuffer(new char[DecodeBufferSize]), m_done(false)
{
}
// 0x080f6670
OggDecodingThreadAL::~OggDecodingThreadAL()
{
    delete[] m_pDecodeBuffer;
}
// 0x080f6100
void OggDecodingThreadAL::run()
{
    for (;;)
    {
        m_fillRequest->wait();
        bool running = true;
        for (;;)
        {
            ALuint buffer = 0;
            m_mutex->lock();
            if (m_freeBuffers.size() > 0)
            {
                buffer = m_freeBuffers[m_freeBuffers.size() - 1];
                m_freeBuffers.pop_back();
                if (buffer == 0)
                    running = false;
            }
            m_mutex->unlock();
            if (buffer == 0)
                break;
            int size = 0;
            for (;;)
            {
                int bitstream;
                int read = ov_read(&m_file, m_pDecodeBuffer + size, DecodeBufferSize - size, 0, 2,
                                   1, &bitstream);
                if (read <= 0)
                {
                    if (read != 0)
                        debugPrint("AudioStreamAL: error while reading (%d)\n", read);
                    else if (getenv("GRIMROCK_DEBUG_AUDIO"))
                        debugPrint("AudioStreamAL: end of stream, loop %d\n", m_loop);
                    if (m_loop)
                        ov_raw_seek(&m_file, 0);
                    else
                        running = false;
                    break;
                }
                size += read;
                if (size >= DecodeBufferSize)
                    break;
            }
            alBufferData(buffer, AL_FORMAT_STEREO16, m_pDecodeBuffer, size, m_file.vi->rate);
            alSourceQueueBuffers(m_source, 1, &buffer);
        }
        if (!running)
        {
            m_finished->signal();
            m_done = true;
            return;
        }
    }
}

// ---- AudioStreamAL ---------------------------------------------------------------

// 0x080f7050
AudioStreamAL::AudioStreamAL(ALuint source, const char* filename, bool loop)
    : m_pThread(new OggDecodingThreadAL)
{
    m_pThread->m_loop = loop;
    m_pThread->m_source = source;
    File* file = openRead(filename);
    ov_callbacks callbacks = {vorbisReadFunc, vorbisSeekFunc, vorbisCloseFunc, vorbisTellFunc};
    if (ov_test_callbacks(file, &m_pThread->m_file, 0, 0, callbacks) < 0)
        throw Exception("Not an Ogg Vorbis file");
    if (ov_test_open(&m_pThread->m_file) < 0)
        throw Exception("Couldn't open Ogg Vorbis file");
    alGenBuffers(3, m_buffers);
    for (int i = 0; i < 3; ++i)
        m_pThread->m_freeBuffers.push_back(m_buffers[i]);
    if (!m_pThread->start())
        throw Exception("Could not create thread");
    m_pThread->m_fillRequest->signal();
}
// 0x080f5e60: stops the decoder, then frees the buffers.
AudioStreamAL::~AudioStreamAL()
{
    m_pThread->m_mutex->lock();
    m_pThread->m_freeBuffers.clear();
    m_pThread->m_freeBuffers.push_back(0);
    m_pThread->m_mutex->unlock();
    m_pThread->m_fillRequest->signal();
    m_pThread->m_finished->wait();
    m_pThread->join();
    alSourceStop(m_pThread->m_source);
    alSourcei(m_pThread->m_source, AL_BUFFER, 0);
    alDeleteBuffers(3, m_buffers);
    ov_clear(&m_pThread->m_file);
    delete m_pThread;
}
// 0x080f4f90
bool AudioStreamAL::isPlaying()
{
    ALint state;
    alGetSourcei(m_pThread->m_source, AL_SOURCE_STATE, &state);
    if (state == AL_PLAYING)
        return true;
    return !m_pThread->m_done;
}
// 0x080f6a90
void AudioStreamAL::update()
{
    ALint processed = 0;
    alGetSourcei(m_pThread->m_source, AL_BUFFERS_PROCESSED, &processed);
    if (processed > 0)
    {
        ALuint buffers[3];
        alSourceUnqueueBuffers(m_pThread->m_source, processed, buffers);
        m_pThread->m_mutex->lock();
        for (int i = 0; i < processed; ++i)
            m_pThread->m_freeBuffers.push_back(buffers[i]);
        m_pThread->m_mutex->unlock();
        m_pThread->m_fillRequest->signal();
    }
}

// ---- SoundSourceAL ---------------------------------------------------------------

// 0x080f5010
SoundSourceAL::SoundSourceAL(AudioWorldAL* world)
    : m_state(Stopped), m_source(0), m_pWorld(world), m_pStream(0), m_positional(false),
      m_volume(1.0f), m_mute(false), m_minDistance(DefaultMinDistance),
      m_maxDistance(DefaultMaxDistance), m_loop(false)
#if GRIMROCK_GAME >= 2
      ,
      m_pitch(1.0f), m_startSample(0), m_finished(false)
#endif
{
}
// 0x080f6720
SoundSourceAL::~SoundSourceAL()
{
    delete m_pStream;
    m_pStream = 0;
    if (m_source)
    {
        alSourceStop(m_source);
        alDeleteSources(1, &m_source);
        m_source = 0;
    }
    m_state = Stopped;
    if (m_pWorld)
        m_pWorld->soundSourceDeleted(this);
}
// 0x080f6930
void SoundSourceAL::play(Sample& sample, bool positional)
{
    stop();
    m_positional = positional;
    m_sample.reset(&sample);
    alGenSources(1, &m_source);
    if (!alIsSource(m_source))
        throw alError("alGenSources");
    alSourcei(m_source, AL_BUFFER, ((SampleAL&)sample).getBuffer());
    alSourcei(m_source, AL_LOOPING, m_loop);
    alSourcei(m_source, AL_REFERENCE_DISTANCE, (int)lrintf(m_minDistance));
    alSourcei(m_source, AL_MAX_DISTANCE, (int)lrintf(m_maxDistance));
#if GRIMROCK_GAME >= 2
    alSourcef(m_source, AL_PITCH, m_pitch);
    if (m_startSample > 0)
        alSourcei(m_source, AL_SAMPLE_OFFSET, m_startSample);
    m_startSample = 0;
    m_finished = false;
#endif
    m_state = Starting;
}
#if GRIMROCK_GAME >= 2
// 0x004c7a00: the buffer is played from startSample (XAUDIO2_BUFFER.PlayBegin)
void SoundSourceAL::play(Sample& sample, bool positional, int startSample)
{
    m_startSample = startSample;
    play(sample, positional);
}
// 0x004c6540
void SoundSourceAL::setPitch(float pitch)
{
    m_pitch = pitch;
    if (m_source)
        alSourcef(m_source, AL_PITCH, pitch);
}
// 0x004c6570: a stopped source that still holds its voice ran to the end by itself
SoundSource::PlayState SoundSourceAL::getPlayState() const
{
    if (m_state == Playing || m_state == Starting)
        return Play_Playing;
    if (m_state == Stopped && m_finished)
        return Play_Finished;
    return Play_Stopped;
}
#endif
// 0x080f6370
void SoundSourceAL::playStream(const char* filename)
{
    stop();
    m_sample.reset(0);
    alGenSources(1, &m_source);
    // streams always loop (the original inlines AudioStreamAL(source, filename, true)),
    // the Loop property of the source only applies to samples
    m_pStream = new AudioStreamAL(m_source, filename, true);
    if (getenv("GRIMROCK_DEBUG_AUDIO"))
        debugPrint("AudioStreamAL: playStream %s on source %u\n", filename, m_source);
    m_state = Starting;
}
// 0x080f6090
void SoundSourceAL::stop()
{
    delete m_pStream;
    m_pStream = 0;
    if (m_source)
    {
        alSourceStop(m_source);
        alDeleteSources(1, &m_source);
        m_source = 0;
    }
    m_state = Stopped;
}
// 0x080f4fe0
int SoundSourceAL::getSamplePosition() const
{
    ALint pos = 0;
    alGetSourcei(m_source, AL_SAMPLE_OFFSET, &pos);
    return pos;
}
// 0x080f54b0
void SoundSourceAL::start(AudioListener& listener)
{
    if (!m_pStream && !m_source)
    {
        m_state = Playing;
        return;
    }
    if (!m_pStream && m_positional && m_pNode)
    {
        const Vec3& p = m_pNode->getLocalToWorldMatrix().pos;
        alSourcefv(m_source, AL_POSITION, &p.x);
        alSourcei(m_source, AL_SOURCE_RELATIVE, AL_FALSE);
    }
    else
    {
        float zero[3] = {0, 0, 0};
        alSourcefv(m_source, AL_POSITION, zero);
        alSourcei(m_source, AL_SOURCE_RELATIVE, AL_TRUE);
    }
    updateVolume(listener);
    alSourcePlay(m_source);
    m_state = Playing;
}
// 0x080f5480
void SoundSourceAL::suspend()
{
    if (m_source)
        alSourcePause(m_source);
    m_state = Suspended;
}
// 0x080f6c00
void SoundSourceAL::update(AudioListener& listener)
{
    if (!m_pStream)
    {
        if (!m_source)
            return;
        if (m_positional && m_pNode)
        {
            const Vec3& p = m_pNode->getLocalToWorldMatrix().pos;
            alSourcefv(m_source, AL_POSITION, &p.x);
        }
        updateVolume(listener);
        ALint state;
        alGetSourcei(m_source, AL_SOURCE_STATE, &state);
        if (state != AL_STOPPED)
            return;
    }
    else
    {
        updateVolume(listener);
        ALint state;
        alGetSourcei(m_source, AL_SOURCE_STATE, &state);
        // the source starves when the decoder falls behind, restart it
        if (state == AL_STOPPED || state == AL_INITIAL)
        {
            if (state == AL_STOPPED && getenv("GRIMROCK_DEBUG_AUDIO"))
                debugPrint("AudioStreamAL: source %u starved, restarting\n", m_source);
            alSourcePlay(m_source);
        }
        m_pStream->update();
        alGetSourcei(m_pStream->getThread()->m_source, AL_SOURCE_STATE, &state);
        if (state == AL_PLAYING)
            return;
        if (!m_pStream->getThread()->m_done)
            return;
    }
    m_state = Stopped;
#if GRIMROCK_GAME >= 2
    m_finished = true;
#endif
}
// 0x080f5360: distance attenuation between min and max distance.
void SoundSourceAL::updateVolume(AudioListener& listener)
{
    if (!m_source)
        return;
    float gain;
    if (!m_positional || !m_pNode)
    {
        gain = 1.0f;
    }
    else
    {
        const Vec3& sourcePos = m_pNode->getLocalToWorldMatrix().pos;
        const Vec3& listenerPos = listener.getLocalToWorldMatrix().pos;
        float dist = std::sqrt((sourcePos.z - listenerPos.z) * (sourcePos.z - listenerPos.z) +
                               (sourcePos.y - listenerPos.y) * (sourcePos.y - listenerPos.y) +
                               (sourcePos.x - listenerPos.x) * (sourcePos.x - listenerPos.x));
        // t in [0,1] between min and max distance; the curve is 1/(16t+1) rescaled so that
        // it is 1 at t=0 and 0 at t=1 (the clamped end values are the curve's own limits)
        float t = (dist - m_minDistance) / (m_maxDistance - m_minDistance);
        if (t < 0.0f)
        {
            gain = AttenuationGainNear;
        }
        else if (t > 1.0f)
        {
            gain = AttenuationGainFar;
        }
        else
        {
            gain =
                (1.0f / (t * AttenuationSteepness + 1.0f) - AttenuationOffset) * AttenuationScale;
            if (gain < 0.0f)
                gain = 0.0f;
        }
        if (gain > 1.0f)
            gain = 1.0f;
    }
    if (m_mute)
        gain = 0.0f;
    else
        gain = m_volume * m_pWorld->m_volume * gain;
    alSourcef(m_source, AL_GAIN, gain);
}

// ---- AudioWorldAL ----------------------------------------------------------------

// 0x080f4e30
AudioWorldAL::AudioWorldAL() : m_rendered(false), m_volume(1.0f) {}
// 0x080f5b00
AudioWorldAL::~AudioWorldAL()
{
    for (int i = 0; i < m_sources.size(); ++i)
    {
        m_sources[i]->stop();
        m_sources[i]->detachWorld();
    }
    ((AudioEngineAL*)AudioEngine::sm_pActiveAudioEngine)->worldDeleted(this);
}
// 0x080f59c0
SoundSource* AudioWorldAL::createSoundSource()
{
    SoundSourceAL* source = new SoundSourceAL(this);
    m_sources.push_back(source);
    return source;
}
// 0x080f4e60
void AudioWorldAL::soundSourceDeleted(SoundSourceAL* source)
{
    m_sources.remove(source);
}

// ---- AudioEngineAL ---------------------------------------------------------------

// 0x080f6e70
AudioEngineAL::AudioEngineAL() : m_pDevice(0), m_pContext(0)
{
    m_pDevice = alcOpenDevice(0);
    if (!m_pDevice)
        throw Exception("Failed to open OpenAL device");
    m_pContext = alcCreateContext(m_pDevice, 0);
    if (!m_pContext)
        throw alcError(m_pDevice, "alcCreateContext");
    if (!alcMakeContextCurrent(m_pContext))
        throw alcError(m_pDevice, "alcMakeContextCurrent");
    alDistanceModel(AL_NONE);
}
// 0x080f5890
AudioEngineAL::~AudioEngineAL()
{
    alcMakeContextCurrent(0);
    if (m_pContext)
        alcDestroyContext(m_pContext);
    alcCloseDevice(m_pDevice);
}
// 0x080f50a0
AudioWorld* AudioEngineAL::createWorld()
{
    AudioWorldAL* world = new AudioWorldAL;
    m_worlds.push_back(world);
    return world;
}
// 0x080f5840
AudioListener* AudioEngineAL::createListener()
{
    return new AudioListenerAL;
}
// 0x080f5800
Sample* AudioEngineAL::createSample()
{
    return new SampleAL;
}
// 0x080f4d60
void AudioEngineAL::beginFrame()
{
    for (int i = 0; i < m_worlds.size(); ++i)
        m_worlds[i]->m_rendered = false;
}
// 0x080f6d50
void AudioEngineAL::render(AudioWorld& world_, AudioListener& listener)
{
    AudioWorldAL& world = (AudioWorldAL&)world_;
    world.m_rendered = true;
    const Matrix4x3& m = listener.getLocalToWorldMatrix();
    float orientation[6] = {-m.z.x, -m.z.y, -m.z.z, m.y.x, m.y.y, m.y.z};
    alListenerfv(AL_POSITION, &m.pos.x);
    alListenerfv(AL_ORIENTATION, orientation);
    alListenerf(AL_GAIN, listener.getVolume());
    for (int i = 0; i < world.m_sources.size(); ++i)
    {
        SoundSourceAL* source = world.m_sources[i];
        switch (source->getState())
        {
        case SoundSourceAL::Starting:
        case SoundSourceAL::Suspended:
            source->start(listener);
            break;
        case SoundSourceAL::Playing:
            source->update(listener);
            break;
        default:
            break;
        }
    }
}
// 0x080f5910: pauses the sources of worlds that were not rendered.
void AudioEngineAL::endFrame()
{
    for (int w = 0; w < m_worlds.size(); ++w)
    {
        AudioWorldAL* world = m_worlds[w];
        if (world->m_rendered)
            continue;
        for (int i = 0; i < world->m_sources.size(); ++i)
        {
            SoundSourceAL* source = world->m_sources[i];
            if (source->getState() == SoundSourceAL::Starting ||
                source->getState() == SoundSourceAL::Playing)
                source->suspend();
        }
    }
}
// 0x080f4d90
void AudioEngineAL::worldDeleted(AudioWorldAL* world)
{
    m_worlds.remove(world);
}

} // namespace engine
