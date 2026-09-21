// Reconstructed from Grimrock.bin.x86 Animation.cpp.
#include "engine/Animation.h"
#include "core/Exception.h"
#include "core/FileStream.h"
#include "core/Matrix.h"
#include "core/StringPool.h"
#include "engine/Node.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace engine
{

using namespace core;

// Node names shared by all animations and controllers.
static StringPool g_nodeStringPool;

Array<Animation*> Animation::sm_animations;

constexpr unsigned int AnimationFormatTag = 0x4d494e41; // "ANIM"
constexpr unsigned int AnimationFormatVersion = 1;

// 0x080d3a30
Animation::Animation() : m_frameRate(0.0f), m_frameCount(0)
{
    sm_animations.push_back(this);
}
// 0x080d3ed0
Animation::~Animation()
{
    for (int i = 0; i < m_tracks.size(); ++i)
        delete m_tracks[i];
    sm_animations.remove(this);
}
// 0x080d4410
void Animation::addTrackItem(const char* nodeName, const Array<TrackKey>& keys)
{
    TrackItem* item = new TrackItem;
    item->nodeName = nodeName;
    item->nodeIndex = g_nodeStringPool.intern(nodeName);
    item->keys = keys;
    m_tracks.push_back(item);
}
// 0x080d4840: ignores duplicates.
void Animation::addEvent(float time, const char* name)
{
    for (int i = 0; i < m_events.size(); ++i)
        if (m_events[i].time == time && strcmp(m_events[i].name.c_str(), name) == 0)
            return;
    Event& e = m_events.push_back();
    e.time = time;
    e.name = name;
}
// 0x080d3bc0
Animation* Animation::getAnimationByFilename(const char* filename)
{
    for (int i = 0; i < sm_animations.size(); ++i)
        if (strcmp(sm_animations[i]->m_filename.c_str(), filename) == 0)
            return sm_animations[i];
    return 0;
}

// 0x080d3900
void AnimationState::advance(float dt)
{
    m_time += dt * m_speed;
    if (!m_loop && m_time > m_animation->getDuration())
        m_playing = false;
}

// 0x080d4f30
AnimationController::AnimationController(Node* root) : m_pRoot(root)
{
    initNodes(root);
    m_states.reserve(16);
    m_events.reserve(64);
}
// 0x080d4c30
AnimationController::~AnimationController() {}

// 0x080d4cd0: maps every node of the hierarchy by its pooled name index.
void AnimationController::initNodes(Node* node)
{
    int index = g_nodeStringPool.intern(node->getName().c_str());
    if (index >= m_nodes.size())
        m_nodes.resize(index + 1, (Node*)0);
    m_nodes[index] = node;
    for (Node* child = node->getFirstChild(); child; child = child->getNextSibling())
        initNodes(child);
}

// 0x080d4960: replaces the animation of an existing clip with that name.
void AnimationController::addClip(Animation& animation, const char* name)
{
    AnimationState* state = 0;
    for (int i = 0; i < m_states.size(); ++i)
    {
        if (strcmp(m_states[i]->m_name.c_str(), name) == 0)
        {
            state = m_states[i].get();
            break;
        }
    }
    if (!state)
    {
        state = new AnimationState;
        m_states.push_back(SharedPtr<AnimationState>(state));
    }
    state->m_animation.reset(&animation);
    state->m_name = name;
}
// 0x080d3940
void AnimationController::play(bool loop)
{
    for (int i = 0; i < m_states.size(); ++i)
        m_states[i]->m_playing = false;
    if (m_states.size() > 0)
    {
        AnimationState* state = m_states[0].get();
        state->m_layer = 0;
        state->m_time = 0.0f;
        state->m_weight = 1.0f;
        state->m_loop = loop;
        state->m_playing = true;
    }
}
// 0x080d3d70: stops the other clips on the same layer.
bool AnimationController::play(const char* name, bool loop, int layer)
{
    for (int i = 0; i < m_states.size(); ++i)
        if (m_states[i]->m_layer == layer)
            m_states[i]->m_playing = false;
    for (int i = 0; i < m_states.size(); ++i)
    {
        AnimationState* state = m_states[i].get();
        if (strcmp(state->m_name.c_str(), name) == 0)
        {
            state->m_time = 0.0f;
            state->m_weight = 1.0f;
            state->m_layer = layer;
            state->m_playing = true;
            state->m_loop = loop;
            return true;
        }
    }
    return false;
}
// 0x080d39b0
void AnimationController::stop()
{
    for (int i = 0; i < m_states.size(); ++i)
        m_states[i]->m_playing = false;
}
// 0x080d3e40
bool AnimationController::isPlaying(const char* name)
{
    for (int i = 0; i < m_states.size(); ++i)
    {
        AnimationState* state = m_states[i].get();
        if (strcmp(state->m_name.c_str(), name) == 0)
            return state->m_playing && state->m_time < state->m_animation->getDuration();
    }
    return false;
}
// 0x080d3b50
AnimationState* AnimationController::getAnimationState(const char* name)
{
    for (int i = 0; i < m_states.size(); ++i)
        if (strcmp(m_states[i]->m_name.c_str(), name) == 0)
            return m_states[i].get();
    return 0;
}

// 0x080d62a0
void AnimationController::update(float dt)
{
    sample();
    advance(dt);
}

// 0x080d4110
void AnimationController::advance(float dt)
{
    m_events.clear();
    for (int i = 0; i < m_states.size(); ++i)
    {
        AnimationState* state = m_states[i].get();
        if (!state->m_playing)
            continue;
        const Animation* anim = state->m_animation.get();
        float duration = anim->getDuration();
        float time = state->m_time;
        if (state->m_loop)
            time = std::fmod(time, duration);
        const Array<Animation::Event>& events = anim->getEvents();
        for (int e = 0; e < events.size(); ++e)
            if (time <= events[e].time && events[e].time < time + dt * state->m_speed)
                m_events.push_back(&events[e]);
        state->m_time += state->m_speed * dt;
        if (!state->m_loop && state->m_time > duration)
            state->m_playing = false;
    }
}

// 0x080d3a10: higher layers first
static bool sortByLayer(AnimationState* a, AnimationState* b)
{
    return b->m_layer < a->m_layer;
}

// 0x080d5910
void AnimationController::sample()
{
    static constexpr int MaxStates = 16;
    AnimationState* states[MaxStates];
    float weights[MaxStates];
    int count = 0;
    for (int i = 0; i < m_states.size() && count < MaxStates; ++i)
    {
        AnimationState* state = m_states[i].get();
        if (state->m_playing && state->m_weight != 0.0f)
            states[count++] = state;
    }
    if (count == 0)
        return;
    if (count > 1)
        std::sort(states, states + count, sortByLayer);

    // higher layers take their weight first, the lowest one gets what is left
    float remaining = 1.0f;
    for (int i = 0; i < count && remaining > 0.0f;)
    {
        int end = i + 1;
        while (end < count && states[end]->m_layer == states[i]->m_layer)
            ++end;
        float sum = 0.0f;
        for (int k = i; k < end; ++k)
            sum += states[k]->m_weight;
        if (sum >= remaining || end == count)
        {
            for (int k = i; k < end; ++k)
                weights[k] = states[k]->m_weight * (remaining / sum);
            count = end;
            remaining = 0.0f;
        }
        else
        {
            for (int k = i; k < end; ++k)
                weights[k] = states[k]->m_weight;
            remaining -= sum;
        }
        i = end;
    }

    // 44 byte accumulator per node: scale, rotation, position, total weight
    struct Accum
    {
        Vec3 scale;
        Quat rot;
        Vec3 pos;
        float weight;
    };
    Array<Accum> accum;
    accum.resize(m_nodes.size());
    memset((void*)accum.data(), 0, sizeof(Accum) * accum.size());
    for (int i = 0; i < count; ++i)
    {
        const AnimationState* state = states[i];
        const Animation* anim = state->m_animation.get();
        int frameCount = anim->getFrameCount();
        if (frameCount == 0)
            continue;
        float frame = anim->getFrameRate() * state->m_time;
        int f0 = (int)frame; // truncated
        int f1 = f0 + 1;
        float frac = frame - (float)f0;
        if (!state->m_loop)
        {
            if (f0 > frameCount - 1)
                f0 = frameCount - 1;
            if (f1 > frameCount - 1)
                f1 = frameCount - 1;
        }
        else
        {
            f0 = f0 < 0 ? frameCount - 1 + (f0 + 1) % frameCount : f0 % frameCount;
            f1 = f1 < 0 ? frameCount - 1 + (f1 + 1) % frameCount : f1 % frameCount;
        }
        const Array<Animation::TrackItem*>& tracks = anim->getTracks();
        for (int t = 0; t < tracks.size(); ++t)
        {
            const Animation::TrackItem* track = tracks[t];
            int node = track->nodeIndex;
            if (node < 0 || node >= m_nodes.size() || !m_nodes[node])
                continue;
            const Animation::TrackKey& k0 = track->keys[f0];
            const Animation::TrackKey& k1 = track->keys[f1];
            Accum& a = accum[node];
            float w = weights[i];
            float w0 = (1.0f - frac) * w, w1 = frac * w;
            a.scale += k0.scale * w0 + k1.scale * w1;
            float wr = w1;
            if (k0.rot.x * k1.rot.x + k0.rot.y * k1.rot.y + k0.rot.z * k1.rot.z +
                    k0.rot.w * k1.rot.w <
                0.0f)
                wr = -w1;
            a.rot.x += k0.rot.x * w0 + k1.rot.x * wr;
            a.rot.y += k0.rot.y * w0 + k1.rot.y * wr;
            a.rot.z += k0.rot.z * w0 + k1.rot.z * wr;
            a.rot.w += k0.rot.w * w0 + k1.rot.w * wr;
            a.pos += k0.pos * w0 + k1.pos * w1;
            a.weight += w;
        }
    }
    for (int n = 0; n < m_nodes.size(); ++n)
    {
        Node* node = m_nodes[n];
        const Accum& a = accum[n];
        if (!node || a.weight <= 0.0f)
            continue;
        float inv = 1.0f / a.weight;
        float rq = 1.0f / std::sqrt(a.rot.x * a.rot.x + a.rot.y * a.rot.y + a.rot.z * a.rot.z +
                                    a.rot.w * a.rot.w);
        float x = a.rot.x * rq, y = a.rot.y * rq, z = a.rot.z * rq, w = a.rot.w * rq;
        float xx2 = 2 * x * x, yy2 = 2 * y * y, zz2 = 2 * z * z;
        float xy2 = 2 * x * y, xz2 = 2 * x * z, yz2 = 2 * y * z;
        float xw2 = 2 * x * w, yw2 = 2 * y * w, zw2 = 2 * z * w;
        float sx = a.scale.x * inv, sy = a.scale.y * inv, sz = a.scale.z * inv;
        Matrix4x3 m;
        m.x.set((1.0f - (yy2 + zz2)) * sx, (xy2 + zw2) * sx, (xz2 - yw2) * sx);
        m.y.set((xy2 - zw2) * sy, (1.0f - (zz2 + xx2)) * sy, (yz2 + xw2) * sy);
        m.z.set((xz2 + yw2) * sz, (yz2 - xw2) * sz, (1.0f - (xx2 + yy2)) * sz);
        m.pos = a.pos * inv;
        node->setLocalMatrix(m);
    }
}

// 0x080d47c0
AnimationAssetProcessor::AnimationAssetProcessor()
{
    setExtensions(AnimationAsset, "", "animation");
}
AnimationAssetProcessor::~AnimationAssetProcessor() {}
// 0x080d4080: no source formats are supported.
void AnimationAssetProcessor::processSingleFile(const char* source, const char* native)
{
    throw Exception("Unknown file format: %s", source);
}

// 0x080d5130
Animation* loadAnimation(const char* filename)
{
    Animation* cached = Animation::getAnimationByFilename(filename);
    if (cached)
        return cached;
    AssetProcessor* processor = findAssetProcessor(AssetProcessor::AnimationAsset, filename);
    processor->processFile(filename);
    String native = processor->getNativeFile(filename);
    FileInputStream in(native.c_str());
    Animation* anim = new Animation;
    int tag, version;
    in.readInt(tag);
    if (tag != (int)AnimationFormatTag)
        throw Exception("Invalid file format: %s", in.getFilename());
    in.readInt(version);
    if (version != AnimationFormatVersion)
        throw Exception("Invalid file version (expected v%d, got v%d): %s", AnimationFormatVersion,
                        version, in.getFilename());
    in.readString(anim->m_name);
    in.readFloat(anim->m_frameRate);
    in.readInt(anim->m_frameCount);
    int numTracks;
    in.readInt(numTracks);
    for (int t = 0; t < numTracks; ++t)
    {
        String nodeName;
        in.readString(nodeName);
        int numKeys;
        in.readInt(numKeys);
        Array<Animation::TrackKey> keys;
        keys.reserve(numKeys);
        for (int k = 0; k < numKeys; ++k)
        {
            Animation::TrackKey& key = keys.push_back();
            in.readVector3(key.pos);
            in.readQuaternion(key.rot);
            in.readVector3(key.scale);
        }
        anim->addTrackItem(nodeName.c_str(), keys);
    }
    anim->m_filename = filename;
    return anim;
}

} // namespace engine
