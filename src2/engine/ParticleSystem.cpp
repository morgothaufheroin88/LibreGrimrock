// Reconstructed from grimrock2.exe ParticleSystem.cpp.
#include "engine/ParticleSystem.h"
#include "core/Exception.h"
#include "core/Math.h"
#include "core/Sys.h"
#include "engine/Mesh.h"
#include "engine/Scene.h"
#include "engine/SpatialDS.h"
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace engine
{

using namespace core;

static inline float frand()
{
    return (float)rand() / (float)RAND_MAX;
}
static inline float frand(float lo, float hi)
{
    return lo + (hi - lo) * frand();
}

// 0x004a7760: p + v t + g t^2/2, or with drag k: p + g t/k + (v - g/k)(1 - e^-kt)/k
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

// 0x004a9380
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
// 0x004a8220 (inner): first triangle whose cumulative area exceeds the sample.
int MeshCDF::pickRandomTriangle() const
{
    float sample = frand();
    for (int i = 0; i < m_cdf.size(); ++i)
        if (sample < m_cdf[i])
            return i;
    return 0;
}
// uniform barycentric sample
void MeshCDF::pickRandomPoint(int triangle, float& u, float& v, float& w) const
{
    float sqrtSample = std::sqrt(frand());
    u = 1.0f - sqrtSample;
    v = sqrtSample * frand();
    w = 1.0f - u - v;
}

// ---- ParticleEmitter -------------------------------------------------------------

// 0x004a9570
ParticleEmitter::ParticleEmitter()
    : m_emitterShape(BoxShape), m_emissionRate(0), m_emissionTime(0), m_maxParticles(0),
      m_spawnBurst(false), m_velocityMin(0), m_velocityMax(0), m_sprayAngleMin(0),
      m_sprayAngleMax(0), m_frameRate(0), m_frameSize(0), m_frameCount(0), m_lifetimeMin(0),
      m_lifetimeMax(0), m_colorAnimation(false), m_opacity(1.0f), m_fadeIn(0), m_fadeOut(0),
      m_sizeMin(0), m_sizeMax(0), m_airResistance(0), m_rotationSpeed(0),
      m_randomInitialRotation(false), m_blendMode(TranslucentBlend), m_objectSpace(false),
      m_clampToGroundPlane(false), m_depthBias(0)
{
}
// 0x004a98c0
ParticleEmitter::~ParticleEmitter() {}
void ParticleEmitter::emitParticles(ParticleEntity& entity, Particle** particles, int count)
{
    if (m_emitterShape == MeshShape && entity.getMesh())
        meshEmitter(entity, particles, count);
    else
        sprayEmitter(entity, particles, count);
}
// 0x004a7e30: random point in the box, direction inside the spray cone around +y.
void ParticleEmitter::sprayEmitter(ParticleEntity& entity, Particle** particles, int count)
{
    const Matrix4x3& m = entity.getNode()->getLocalToWorldMatrix();
    for (int i = 0; i < count; ++i)
    {
        Particle& p = *particles[i];
        p.pos.set(frand(m_boxMin.x, m_boxMax.x), frand(m_boxMin.y, m_boxMax.y),
                  frand(m_boxMin.z, m_boxMax.z));
        float azimuth = frand() * TWO_PI - PI;
        float angle = frand(m_sprayAngleMin, m_sprayAngleMax) * (PI / 180.0f);
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
// 0x004a8220: random point on the (optionally skinned) mesh surface of the entity,
// velocity along the interpolated normal.
void ParticleEmitter::meshEmitter(ParticleEntity& entity, Particle** particles, int count)
{
    static Matrix4x3 skinningMatrices[256];
    MeshCDF* cdf = entity.getMesh();
    Mesh* mesh = cdf->getMesh();
    const Array<int>& idx = mesh->getIndices();
    const Vec3* pos = (const Vec3*)mesh->getVertexArray(Mesh::Position);
    const Vec3* nrm = (const Vec3*)mesh->getVertexArray(Mesh::Normal);
    const Mesh::VertexArray& bi = mesh->getVertexArrayInfo(Mesh::BoneIndices);
    const Matrix4x3& m = entity.getNode()->getLocalToWorldMatrix();
    Skeleton* skeleton = entity.getSkeleton();
    bool skinned = skeleton && bi.pData && skeleton->getBoneCount() <= 256;
    if (skinned)
        skeleton->computeSkinningMatrices(entity.getNode()->getWorldToLocalMatrix(),
                                          skinningMatrices);
    for (int i = 0; i < count; ++i)
    {
        Particle& p = *particles[i];
        int tri = cdf->pickRandomTriangle();
        float u, v, w;
        cdf->pickRandomPoint(tri, u, v, w);
        int ia = idx[tri * 3], ib = idx[tri * 3 + 1], ic = idx[tri * 3 + 2];
        Vec3 pa = pos[ia], pb = pos[ib], pc = pos[ic];
        Vec3 na = nrm ? nrm[ia] : Vec3(0, 1, 0), nb = nrm ? nrm[ib] : na, nc = nrm ? nrm[ic] : na;
        if (skinned)
        {
            // the first bone of each vertex moves the point and the normal
            const int verts[3] = {ia, ib, ic};
            Vec3* outPos[3] = {&pa, &pb, &pc};
            Vec3* outNrm[3] = {&na, &nb, &nc};
            for (int k = 0; k < 3; ++k)
            {
                int bone = *(const unsigned char*)((const char*)bi.pData + verts[k] * bi.stride);
                *outPos[k] = skinningMatrices[bone].transformPoint(*outPos[k]);
                *outNrm[k] = skinningMatrices[bone].transformVector(*outNrm[k]);
            }
        }
        p.pos = pa * u + pb * v + pc * w;
        Vec3 dir = nrm ? na * u + nb * v + nc * w : cross(pb - pa, pc - pa);
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

ParticleState::ParticleState(ParticleEntity* entity)
    : m_pEntity(entity), m_numEmitted(0), m_numAlive(0), m_time(0.0f)
{
}
ParticleState::~ParticleState() {}
// 0x004a9130
void ParticleState::setEmitter(ParticleEmitter* emitter)
{
    m_emitter.reset(emitter);
    m_particles.reserve(emitter->m_maxParticles);
    m_freeParticles.reserve(emitter->m_maxParticles);
}
// 0x004a9650 (inner): reuse a free slot, grow up to maxParticles, else recycle a random one.
Particle* ParticleState::allocParticle()
{
    if (m_freeParticles.size() > 0)
    {
        int i = m_freeParticles.back();
        m_freeParticles.pop_back();
        ++m_numAlive;
        return &m_particles[i];
    }
    if (m_particles.size() < m_emitter->m_maxParticles)
    {
        ++m_numAlive;
        Particle& p = m_particles.push_back();
        p.pos.set(0, 0, 0);
        p.velocity.set(0, 0, 0);
        return &p;
    }
    return &m_particles[rand() % m_particles.size()];
}
// 0x004a9650: emit up to 1024 particles per call to reach the target count.
void ParticleState::emitParticles(float dt, bool skipEmission)
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
    if (skipEmission)
    {
        // a burst emitter with a long emission time keeps its particles for the burst
        if (!emitter->m_spawnBurst || emitter->m_emissionTime <= 10000.0f)
            m_numEmitted = target;
        return;
    }
    constexpr int EmitBatchSize = 1024;
    Particle* batch[EmitBatchSize];
    while (count > 0)
    {
        int batchCount = count > EmitBatchSize ? EmitBatchSize : count;
        for (int i = 0; i < batchCount; ++i)
            batch[i] = allocParticle();
        emitter->emitParticles(*m_pEntity, batch, batchCount);
        m_numEmitted += batchCount;
        count -= batchCount;
    }
}
// 0x004a8ea0
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
void ParticleState::reset()
{
    m_particles.resize(0);
    m_freeParticles.resize(0);
    m_numEmitted = 0;
    m_numAlive = 0;
    m_time = 0.0f;
}
bool ParticleState::isAlive() const
{
    return m_time < m_emitter->m_emissionTime + m_emitter->m_lifetimeMax;
}

// ---- ParticleSystem --------------------------------------------------------------

// 0x004a9920
ParticleSystem::ParticleSystem() : m_timeStamp(0)
{
    m_emitters.reserve(8);
}
ParticleSystem::~ParticleSystem() {}
// 0x004a99d0
void ParticleSystem::addParticleEmitter(ParticleEmitter* emitter)
{
    m_emitters.push_back(SharedPtr<ParticleEmitter>(emitter));
}
// 0x004a9ac0
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
// 0x004a9980
void ParticleSystem::clear()
{
    m_emitters.clear();
}
// 0x004a7690
void ParticleSystem::updateTimeStamp()
{
    m_timeStamp = sysClock();
}

// ---- ParticleEntity --------------------------------------------------------------

// 0x004aa2e0
ParticleEntity::ParticleEntity(ParticleSystem* system)
    : RenderEntity(ParticleEntityType), m_timeStamp(0), m_emitting(true), m_seed(-1),
      m_groundPlaneY(0.0f), m_opacity(1.0f), m_distanceFadeStart(0.0f), m_distanceFadeEnd(0.0f)
{
    m_bounds.min.set(0, 0, 0);
    m_bounds.max.set(0, 0, 0);
    setParticleSystem(system);
}
// 0x004a9fe0
ParticleEntity::~ParticleEntity()
{
    for (int i = 0; i < m_states.size(); ++i)
        delete m_states[i];
}
// 0x004aa110: one state per emitter; remembers the system's time stamp to detect edits.
void ParticleEntity::setParticleSystem(ParticleSystem* system)
{
    m_particleSystem.reset(system);
    for (int i = 0; i < m_states.size(); ++i)
        delete m_states[i];
    m_states.resize(0);
    m_timeStamp = 0;
    if (system)
    {
        m_timeStamp = system->getTimeStamp();
        const Array<SharedPtr<ParticleEmitter>>& emitters = system->getEmitters();
        for (int i = 0; i < emitters.size(); ++i)
        {
            ParticleState* state = new ParticleState(this);
            state->setEmitter(emitters[i].get());
            m_states.push_back(state);
        }
    }
    reset();
}
void ParticleEntity::setMesh(MeshCDF* mesh)
{
    m_mesh.reset(mesh);
}
void ParticleEntity::setSkeleton(Skeleton* skeleton)
{
    m_skeleton.reset(skeleton);
}
void ParticleEntity::setHeightmap(RenderableTexture* texture)
{
    m_heightmap.reset(texture);
}
AABox3 ParticleEntity::getWorldBounds() const
{
    return m_bounds;
}
// 0x004a8f30
void ParticleEntity::reset()
{
    for (int i = 0; i < m_states.size(); ++i)
        m_states[i]->reset();
    m_seed = -1;
}
// 0x004a76f0
bool ParticleEntity::isAlive() const
{
    for (int i = 0; i < m_states.size(); ++i)
        if (m_states[i]->isAlive())
            return true;
    return false;
}
// 0x004aa260
void ParticleEntity::update(float dt, bool skipEmission)
{
    ParticleSystem* system = m_particleSystem.get();
    if (!system)
        return;
    if (m_timeStamp != system->getTimeStamp())
        setParticleSystem(system);
    for (int i = 0; i < m_states.size(); ++i)
    {
        if (m_emitting)
            m_states[i]->emitParticles(dt, skipEmission);
        m_states[i]->updateParticles(dt);
    }
    if (!skipEmission)
        updateBounds();
}
// 0x004a7840: the bounds are seeded from the first live particle, then grown from a
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
            int i = 0;
            while (i < particles.size() && particles[i].time < 0.0f)
                ++i;
            if (i >= particles.size())
                continue;
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
    notifyBoundChanged();
}

} // namespace engine
