// Keyframe animations and their controller, reconstructed from Animation.cpp
// (0x080d3900-0x080d6c50). Native format "ANIM" version 1.
#pragma once
#include "core/Array.h"
#include "core/SharedPtr.h"
#include "core/String.h"
#include "core/Vector.h"
#include "engine/AssetProcessor.h"

namespace engine
{

class Node;

class Animation
{
  public:
    // 40 bytes
    struct TrackKey
    {
        core::Vec3 pos;
        core::Quat rot;
        core::Vec3 scale;
    };
    struct TrackItem
    {
        core::String nodeName;
        int nodeIndex; // index in the global node name pool
        core::Array<TrackKey> keys;
    };
    struct Event
    {
        core::String name;
        float time;
    };

    Animation();
    ~Animation();
    void addTrackItem(const char* nodeName, const core::Array<TrackKey>& keys);
    void addEvent(float time, const char* name);
#if GRIMROCK_GAME >= 2
    // 0x004a5aa0
    void removeEvents()
    {
        m_events.clear();
    }
#endif
    const core::String& getFilename() const
    {
        return m_filename;
    }
    const core::String& getName() const
    {
        return m_name;
    }
    float getFrameRate() const
    {
        return m_frameRate;
    }
    int getFrameCount() const
    {
        return m_frameCount;
    }
    void setName(const char* name)
    {
        m_name = name;
    }
    void setFrameRate(float rate)
    {
        m_frameRate = rate;
    }
    void setFrameCount(int count)
    {
        m_frameCount = count;
    }
    float getDuration() const
    {
        return m_frameCount / m_frameRate;
    }
    const core::Array<TrackItem*>& getTracks() const
    {
        return m_tracks;
    }
    const core::Array<Event>& getEvents() const
    {
        return m_events;
    }
    static Animation* getAnimationByFilename(const char* filename);
    static core::Array<Animation*> sm_animations;

  private:
    friend Animation* loadAnimation(const char* filename);
    core::String m_filename;
    core::String m_name;
    float m_frameRate;
    int m_frameCount;
    core::Array<TrackItem*> m_tracks;
    core::Array<Event> m_events;
};

// 0x28 bytes: one clip of a controller.
class AnimationState
{
  public:
    AnimationState()
        : m_layer(0), m_time(0.0f), m_weight(0.0f), m_speed(1.0f),
#if GRIMROCK_GAME >= 2
          m_weightSpeed(0.0f),
#endif
          m_loop(false), m_playing(false)
    {
    }
    // 0x080d3900 / 0x004a4af0 (Grimrock 2 also fades the weight for crossfades)
    void advance(float dt);
    Animation* getAnimation() const
    {
        return m_animation.get();
    }
    const core::String& getName() const
    {
        return m_name;
    }

    core::SharedPtr<Animation> m_animation;
    core::String m_name;
    int m_layer;
    float m_time;
    float m_weight;
    float m_speed;
#if GRIMROCK_GAME >= 2
    float m_weightSpeed; // weight change per second, negative while fading out (+0x24)
#endif
    bool m_loop;
    bool m_playing;
};

class AnimationController
{
  public:
#if GRIMROCK_GAME >= 2
    // 0x004a7090: the node hierarchy comes later through bind (0x24 bytes)
    AnimationController();
    // 0x004a5f80
    void bind(Node* root);
    // 0x004a4fa0: fades the clip in over fadeTime seconds while the playing clips of
    // the layer fade out.
    bool crossfade(const char* name, float fadeTime, bool loop, int layer);
#else
    explicit AnimationController(Node* root);
#endif
    ~AnimationController();
    void addClip(Animation& animation, const char* name);
    // Plays the first clip on layer 0.
    void play(bool loop);
    bool play(const char* name, bool loop, int layer);
    void stop();
    bool isPlaying(const char* name);
#if GRIMROCK_GAME >= 2
    // 0x004a4bd0: any clip still playing (looping or before its end)
    bool isPlaying() const;
#endif
    AnimationState* getAnimationState(const char* name);
    AnimationState* getAnimationState(int i)
    {
        return m_states[i].get();
    }
    int getAnimationStateCount() const
    {
        return m_states.size();
    }
    // 0x080d5910: blends the playing clips by layer and weight into the nodes.
    void sample();
    // 0x080d4110: advances the clips and collects the events crossed.
    void advance(float dt);
    void update(float dt);
    const core::Array<const Animation::Event*>& getTriggeredEvents() const
    {
        return m_events;
    }
    Node* getRootNode() const
    {
        return m_pRoot;
    }

  private:
    void initNodes(Node* node);
    Node* m_pRoot;
    core::Array<core::SharedPtr<AnimationState>> m_states;
    core::Array<Node*> m_nodes; // indexed like the node name pool
    core::Array<const Animation::Event*> m_events;
};

class AnimationAssetProcessor : public SingleFileAssetProcessor
{
  public:
    AnimationAssetProcessor();
    ~AnimationAssetProcessor();
    void processSingleFile(const char* source, const char* native);
};

// 0x080d5130: cached by filename.
Animation* loadAnimation(const char* filename);

} // namespace engine
