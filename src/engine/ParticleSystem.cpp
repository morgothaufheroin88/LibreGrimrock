// Reconstructed from Grimrock.bin.x86 ParticleSystem.cpp.
#include "engine/ParticleSystem.h"
#include "core/Exception.h"
#include "core/Math.h"
#include "engine/Mesh.h"
#include "engine/SpatialDS.h"
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace engine
{

using namespace core;

Array<ParticleSystem*> ParticleSystem::sm_particleSystems;

// 0x08107e20 static initialisation: property table for particle definition files.
ParticleEmitter::Property ParticleEmitter::Properties[30] = {
    {"EmissionRate", 1, 0, 0},  {"EmissionTime", 1, 0, 0},
    {"MaxParticles", 0, 0, 0},  {"SpawnBurst", 2, 0, 0},
    {"BoxMin", 5, 0, 0},        {"BoxMax", 5, 0, 0},
    {"Velocity", 6, 0, 0},      {"SprayAngle", 6, 0, 0},
    {"Texture", 4, 0, 0},       {"FrameRate", 1, 0, 0},
    {"FrameSize", 0, 0, 0},     {"FrameCount", 0, 0, 0},
    {"Lifetime", 6, 0, 0},      {"Color0", 5, 0, 0},
    {"Color1", 5, 0, 0},        {"Color2", 5, 0, 0},
    {"Color3", 5, 0, 0},        {"ColorAnimation", 2, 0, 0},
    {"Opacity", 1, 0, 0},       {"FadeIn", 1, 0, 0},
    {"FadeOut", 1, 0, 0},       {"Size", 6, 0, 0},
    {"Gravity", 5, 0, 0},       {"AirResistance", 1, 0, 0},
    {"RotationSpeed", 1, 0, 0}, {"BlendMode", 3, 0, "Translucent|Additive"},
    {"ObjectSpace", 2, 0, 0},   {"ClampToGroundPlane", 2, 0, 0},
    {"DepthBias", 1, 0, 0},     {0, 0, 0, 0}};

static inline float frand()
{
    return (float)rand() / (float)RAND_MAX;
}
static inline float frand(float lo, float hi)
{
    return lo + (hi - lo) * frand();
}

// 0x08108350 (inner): p + v t + g t^2/2, or with drag k: p + g t/k + (v - g/k)(1 - e^-kt)/k
Vec3 evaluateParticlePosition(const Particle& p, const ParticleEmitter& e)
{
    if (e.m_airResistance == 0.0f)
        return p.pos + p.velocity * p.time + e.m_gravity * (0.5f * p.time * p.time);
    float k = e.m_airResistance;
    float invK = 1.0f / k;
    float decay = 1.0f - std::exp(-k * p.time);
    return p.pos + e.m_gravity * (p.time * invK) +
           (p.velocity - e.m_gravity * invK) * (decay * invK);
}

// ---- MeshCDF ---------------------------------------------------------------------

// 0x0810a9a0
MeshCDF::MeshCDF(Mesh* mesh) : m_mesh(mesh)
{
    const Array<int>& idx = mesh->getIndices();
    const Vec3* pos = (const Vec3*)mesh->getVertexArray(Mesh::Position);
    float total = 0.0f;
    for (int i = 0; i + 2 < idx.size(); i += 3)
    {
        Vec3 n = cross(pos[idx[i + 1]] - pos[idx[i]], pos[idx[i + 2]] - pos[idx[i]]);
        total += n.length();
        m_cdf.push_back(total);
    }
    if (total > 0.0f)
        for (int i = 0; i < m_cdf.size(); ++i)
            m_cdf[i] /= total;
}
// 0x08107b30: binary search in the cumulative distribution.
int MeshCDF::pickRandomTriangle() const
{
    if (m_cdf.size() == 0)
        return 0;
    float sample = frand();
    int lo = 0, hi = m_cdf.size() - 1;
    while (lo < hi)
    {
        int mid = (lo + hi) / 2;
        if (m_cdf[mid] < sample)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}
// 0x08107d90: uniform barycentric sample.
void MeshCDF::pickRandomPoint(int triangle, float& u, float& v, float& w) const
{
    float sqrtSample = std::sqrt(frand());
    u = 1.0f - sqrtSample;
    v = sqrtSample * frand();
    w = 1.0f - u - v;
}

// ---- ParticleEmitter -------------------------------------------------------------

// 0x0810ac30
ParticleEmitter::ParticleEmitter()
    : m_emissionRate(0), m_emissionTime(0), m_maxParticles(0), m_spawnBurst(false),
      m_velocityMin(0), m_velocityMax(0), m_sprayAngleMin(0), m_sprayAngleMax(0), m_frameRate(0),
      m_frameSize(0), m_frameCount(0), m_lifetimeMin(0), m_lifetimeMax(0), m_colorAnimation(false),
      m_opacity(1.0f), m_fadeIn(0), m_fadeOut(0), m_sizeMin(0), m_sizeMax(0), m_airResistance(0),
      m_rotationSpeed(0), m_blendMode(TranslucentBlend), m_objectSpace(false),
      m_clampToGroundPlane(false), m_depthBias(0)
{
}
// 0x0810bbd0
ParticleEmitter::~ParticleEmitter() {}
// 0x0810ac10
void ParticleEmitter::setMesh(MeshCDF* mesh)
{
    m_mesh.reset(mesh);
}
// 0x0810bbb0
void ParticleEmitter::setSkeleton(Skeleton* skeleton)
{
    m_skeleton.reset(skeleton);
}
// 0x08109e60
void ParticleEmitter::emitParticles(ParticleEntity& entity, Particle** particles, int count)
{
    if (m_mesh)
        meshEmitter(entity, particles, count);
    else
        sprayEmitter(entity, particles, count);
}
// 0x081088e0: random point in the box, direction inside the spray cone around +y.
void ParticleEmitter::sprayEmitter(ParticleEntity& entity, Particle** particles, int count)
{
    const Matrix4x3& m = entity.getNode()->getLocalToWorldMatrix();
    for (int i = 0; i < count; ++i)
    {
        Particle& p = *particles[i];
        p.pos.set(frand(m_boxMin.x, m_boxMax.x), frand(m_boxMin.y, m_boxMax.y),
                  frand(m_boxMin.z, m_boxMax.z));
        float angle = frand(m_sprayAngleMin, m_sprayAngleMax) * (PI / 180.0f);
        float azimuth = frand(-PI, PI);
        float speed = frand(m_velocityMin, m_velocityMax);
        p.velocity.set(std::sin(angle) * std::cos(azimuth) * speed, std::cos(angle) * speed,
                       std::sin(angle) * std::sin(azimuth) * speed);
        if (!m_objectSpace)
        {
            p.pos = m.transformPoint(p.pos);
            p.velocity = m.transformVector(p.velocity);
        }
        p.time = 0.0f;
        p.lifetime = frand(m_lifetimeMin, m_lifetimeMax);
        p.size = frand(m_sizeMin, m_sizeMax);
        p.random = frand();
    }
}
// 0x08108ee0: random point on the (optionally skinned) mesh surface, velocity along the normal.
void ParticleEmitter::meshEmitter(ParticleEntity& entity, Particle** particles, int count)
{
    static Matrix4x3 skinningMatrices[256];
    Mesh* mesh = m_mesh->getMesh();
    const Array<int>& idx = mesh->getIndices();
    const Vec3* pos = (const Vec3*)mesh->getVertexArray(Mesh::Position);
    const Vec3* nrm = (const Vec3*)mesh->getVertexArray(Mesh::Normal);
    const Mesh::VertexArray& bi = mesh->getVertexArrayInfo(Mesh::BoneIndices);
    const Mesh::VertexArray& bw = mesh->getVertexArrayInfo(Mesh::BoneWeights);
    const Matrix4x3& m = entity.getNode()->getLocalToWorldMatrix();
    bool skinned = m_skeleton && bi.pData && bw.pData && m_skeleton->getBoneCount() <= 256;
    if (skinned)
        m_skeleton->computeSkinningMatrices(entity.getNode()->getWorldToLocalMatrix(),
                                            skinningMatrices);
    for (int i = 0; i < count; ++i)
    {
        Particle& p = *particles[i];
        int tri = m_mesh->pickRandomTriangle();
        float u, v, w;
        m_mesh->pickRandomPoint(tri, u, v, w);
        int ia = idx[tri * 3], ib = idx[tri * 3 + 1], ic = idx[tri * 3 + 2];
        Vec3 pa = pos[ia], pb = pos[ib], pc = pos[ic];
        if (skinned)
        {
            const int verts[3] = {ia, ib, ic};
            Vec3* out[3] = {&pa, &pb, &pc};
            for (int k = 0; k < 3; ++k)
            {
                const unsigned char* bones = (const unsigned char*)bi.pData + verts[k] * bi.stride;
                const float* weights = (const float*)((const char*)bw.pData + verts[k] * bw.stride);
                Vec3 skinned(0, 0, 0);
                for (int b = 0; b < bi.components && b < 4; ++b)
                    if (weights[b] != 0.0f)
                        skinned +=
                            skinningMatrices[bones[b]].transformPoint(pos[verts[k]]) * weights[b];
                *out[k] = skinned;
            }
        }
        p.pos = pa * u + pb * v + pc * w;
        Vec3 dir = nrm ? nrm[ia] * u + nrm[ib] * v + nrm[ic] * w : cross(pb - pa, pc - pa);
        dir.normalize();
        p.velocity = dir * frand(m_velocityMin, m_velocityMax);
        if (!m_objectSpace)
        {
            p.pos = m.transformPoint(p.pos);
            p.velocity = m.transformVector(p.velocity);
        }
        p.time = 0.0f;
        p.lifetime = frand(m_lifetimeMin, m_lifetimeMax);
        p.size = frand(m_sizeMin, m_sizeMax);
        p.random = frand();
    }
}

// ---- ParticleState ---------------------------------------------------------------

// 0x08108880
ParticleState::ParticleState(ParticleEntity* entity)
    : m_pEntity(entity), m_numEmitted(0), m_numAlive(0), m_time(0.0f)
{
}
// 0x0810af10
ParticleState::~ParticleState() {}
// 0x0810b020
void ParticleState::setEmitter(ParticleEmitter* emitter)
{
    m_emitter.reset(emitter);
    m_particles.reserve(emitter->m_maxParticles);
    m_freeParticles.reserve(emitter->m_maxParticles);
}
// 0x08107b90: reuse a free slot, grow up to maxParticles, else recycle a random one.
Particle* ParticleState::allocParticle()
{
    if (m_freeParticles.size() > 0)
    {
        int i = m_freeParticles.back();
        m_freeParticles.pop_back();
        return &m_particles[i];
    }
    if (m_particles.size() < m_emitter->m_maxParticles)
    {
        Particle& p = m_particles.push_back();
        p.pos.set(0, 0, 0);
        p.velocity.set(0, 0, 0);
        return &p;
    }
    --m_numAlive;
    return &m_particles[rand() % m_particles.size()];
}
// 0x0810a1e0: emit up to 1024 particles per call to reach the target count.
void ParticleState::emitParticles(float dt)
{
    m_time += dt;
    ParticleEmitter* emitter = m_emitter.get();
    int target;
    if (emitter->m_spawnBurst)
    {
        target = emitter->m_maxParticles;
    }
    else
    {
        float emissionTime = emitter->m_emissionTime > 0.0f ? emitter->m_emissionTime : FLT_MAX;
        if (m_time < emissionTime)
            emissionTime = m_time;
        target = (int)lrintf(emissionTime * emitter->m_emissionRate);
    }
    int count = target - m_numEmitted;
    if (count < 1)
        return;
    constexpr int EmitBatchSize = 1024;
    Particle* batch[EmitBatchSize];
    while (count > 0)
    {
        int batchCount = count > EmitBatchSize ? EmitBatchSize : count;
        for (int i = 0; i < batchCount; ++i)
            batch[i] = allocParticle();
        emitter->emitParticles(*m_pEntity, batch, batchCount);
        m_numEmitted += batchCount;
        m_numAlive += batchCount;
        count -= batchCount;
    }
}
// 0x08109ea0
void ParticleState::updateParticles(float dt)
{
    for (int i = 0; i < m_particles.size(); ++i)
    {
        Particle& p = m_particles[i];
        if (p.time < 0.0f)
            continue;
        p.time += dt;
        if (p.time > p.lifetime)
        {
            m_freeParticles.push_back(i);
            p.time = -1.0f;
            --m_numAlive;
        }
    }
}
// 0x08109fe0
void ParticleState::reset()
{
    m_particles.resize(0);
    m_freeParticles.resize(0);
    m_numEmitted = 0;
    m_numAlive = 0;
    m_time = 0.0f;
}
// 0x08107a50
bool ParticleState::isAlive() const
{
    return m_time < m_emitter->m_emissionTime + m_emitter->m_lifetimeMax;
}

// ---- ParticleSystem --------------------------------------------------------------

// 0x0810a790
ParticleSystem::ParticleSystem()
{
    sm_particleSystems.push_back(this);
}
// 0x0810b180
ParticleSystem::~ParticleSystem()
{
    sm_particleSystems.remove(this);
}
// 0x0810b340
void ParticleSystem::addParticleEmitter(ParticleEmitter* emitter)
{
    m_emitters.push_back(SharedPtr<ParticleEmitter>(emitter));
}
// 0x0810ad50
void ParticleSystem::removeParticleEmitter(ParticleEmitter* emitter)
{
    for (int i = 0; i < m_emitters.size(); ++i)
    {
        if (m_emitters[i].get() == emitter)
        {
            m_emitters.erase(i);
            return;
        }
    }
}
// 0x08107d20
ParticleSystem* ParticleSystem::getParticleSystemByFilename(const char* filename)
{
    for (int i = 0; i < sm_particleSystems.size(); ++i)
        if (strcmp(sm_particleSystems[i]->m_filename.c_str(), filename) == 0)
            return sm_particleSystems[i];
    return 0;
}
// 0x0810b520: DataDef file of "emitter" blocks following ParticleEmitter::Properties. The
// game defines its particle systems from Lua instead.
void ParticleSystem::load(const char* filename)
{
    m_filename = filename;
    throw Exception("Particle system files are not supported: %s", filename);
}
// 0x0810bad0
ParticleSystem* loadParticleSystem(const char* filename)
{
    ParticleSystem* existing = ParticleSystem::getParticleSystemByFilename(filename);
    if (existing)
        return existing;
    ParticleSystem* system = new ParticleSystem;
    system->load(filename);
    return system;
}

// ---- ParticleEntity --------------------------------------------------------------

// 0x0810c4e0
ParticleEntity::ParticleEntity(ParticleSystem* system)
    : RenderEntity(ParticleEntityType), m_emitterListData(0), m_emitterListSize(0),
      m_emitting(true), m_seed(-1)
{
    m_bounds.min.set(0, 0, 0);
    m_bounds.max.set(0, 0, 0);
    setParticleSystem(system);
}
// 0x0810bdf0
ParticleEntity::~ParticleEntity()
{
    for (int i = 0; i < m_states.size(); ++i)
        delete m_states[i];
}
// 0x0810c0b0: one state per emitter; remembers the emitter list to detect edits.
void ParticleEntity::setParticleSystem(ParticleSystem* system)
{
    m_particleSystem.reset(system);
    for (int i = 0; i < m_states.size(); ++i)
        delete m_states[i];
    m_states.resize(0);
    m_emitterListData = 0;
    m_emitterListSize = 0;
    if (system)
    {
        const Array<SharedPtr<ParticleEmitter>>& emitters = system->getEmitters();
        for (int i = 0; i < emitters.size(); ++i)
        {
            ParticleState* state = new ParticleState(this);
            state->setEmitter(emitters[i].get());
            m_states.push_back(state);
        }
        m_emitterListData = emitters.data();
        m_emitterListSize = emitters.size();
    }
    reset();
}
// 0x08107b00
AABox3 ParticleEntity::getWorldBounds()
{
    return m_bounds;
}
// 0x0810a500
void ParticleEntity::reset()
{
    for (int i = 0; i < m_states.size(); ++i)
        m_states[i]->reset();
    m_seed = -1;
}
// 0x08107a70
bool ParticleEntity::isAlive() const
{
    for (int i = 0; i < m_states.size(); ++i)
        if (m_states[i]->isAlive())
            return true;
    return false;
}
// 0x0810c5b0
void ParticleEntity::update(float dt)
{
    ParticleSystem* system = m_particleSystem.get();
    if (!system)
        return;
    if (m_emitterListData != system->getEmitters().data() ||
        m_emitterListSize != system->getEmitters().size())
        setParticleSystem(system);
    for (int i = 0; i < m_states.size(); ++i)
    {
        if (m_emitting)
            m_states[i]->emitParticles(dt);
        m_states[i]->updateParticles(dt);
    }
    updateBounds();
}
// 0x08108350: the bounds are seeded from the first live particle, then grown from a
// rotating sample of at most MaxBoundsSamples particles per state; every
// BoundsResetInterval-th update starts over from an empty box and visits every particle.
constexpr int MaxBoundsSamples = 50;
constexpr unsigned BoundsResetInterval = 100;
constexpr float EmptyBoundsExtent = 100000.0f;

void ParticleEntity::updateBounds()
{
    if (m_states.size() < 1)
        return;
    int alive = 0;
    for (int i = 0; i < m_states.size(); ++i)
        alive += m_states[i]->getNumAlive();
    if (alive == 0)
        return;
    const Matrix4x3& localToWorld = m_pNode->getLocalToWorldMatrix();
    unsigned counter = (unsigned)m_seed;
    if ((int)counter < 0)
    {
        for (int s = 0; s < m_states.size(); ++s)
        {
            ParticleState* state = m_states[s];
            const ParticleEmitter& emitter = *state->getEmitter();
            Array<Particle>& particles = state->getParticles();
            if (particles.size() < 1)
                break;
            int i = 0;
            while (particles[i].time < 0.0f)
            {
                if (++i >= particles.size())
                    break;
            }
            if (i >= particles.size())
                break;
            Vec3 pos = evaluateParticlePosition(particles[i], emitter);
            if (emitter.m_objectSpace)
                pos = localToWorld.transformPoint(pos);
            if (emitter.m_clampToGroundPlane && pos.y <= 0.0f)
                pos.y = 0.0f;
            m_bounds.min = pos;
            m_bounds.max = pos;
            break;
        }
        counter = rand() & 0xffff;
        m_seed = (int)counter;
    }
    for (int s = 0; s < m_states.size(); ++s)
    {
        ParticleState* state = m_states[s];
        if (state->getNumAlive() <= 0)
            continue;
        const ParticleEmitter& emitter = *state->getEmitter();
        Array<Particle>& particles = state->getParticles();
        int numParticles = particles.size();
        int samples = numParticles < MaxBoundsSamples ? numParticles : MaxBoundsSamples;
        int index = numParticles > 0 ? (int)counter % numParticles : 0;
        int stride;
        if (counter % BoundsResetInterval == 0)
        {
            m_bounds.min.set(EmptyBoundsExtent, EmptyBoundsExtent, EmptyBoundsExtent);
            m_bounds.max.set(-EmptyBoundsExtent, -EmptyBoundsExtent, -EmptyBoundsExtent);
            index = 0;
            stride = 1;
            samples = numParticles;
        }
        else
        {
            stride = samples > 0 ? numParticles / samples : 1;
            if (stride < 1)
                stride = 1;
        }
        float size = emitter.m_sizeMax;
        for (int i = 0; i < samples; ++i)
        {
            const Particle& particle = particles[index];
            if (particle.time >= 0.0f)
            {
                Vec3 pos = evaluateParticlePosition(particle, emitter);
                if (emitter.m_objectSpace)
                    pos = localToWorld.transformPoint(pos);
                if (emitter.m_clampToGroundPlane && pos.y <= 0.0f)
                    pos.y = 0.0f;
                Vec3 extent(size, size, size);
                m_bounds.min = minVec(m_bounds.min, pos - extent);
                m_bounds.max = maxVec(m_bounds.max, pos + extent);
            }
            index = (index + stride) % numParticles;
        }
    }
    m_seed = (int)(counter + 1);
    if (m_pSpatialNode)
        m_pSpatialNode->notifyBoundChanged();
}

} // namespace engine
