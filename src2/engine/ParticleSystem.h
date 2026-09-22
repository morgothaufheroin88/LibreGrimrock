// Particle systems of Legend of Grimrock 2, reconstructed from grimrock2.exe
// ParticleSystem.cpp (0x004a7690-0x004aa3a0). Unlike the first game the emitter mesh and
// skeleton belong to the entity, emitters have a shape enum and a random initial
// rotation, entities fade with distance and can clamp to a height map, and particle
// systems carry a time stamp that entities use to notice edits.
#pragma once
#include "core/Array.h"
#include "core/Prim.h"
#include "core/SharedPtr.h"
#include "engine/RenderEntity.h"

namespace engine
{

class Mesh;
class RenderableTexture;
class ParticleEntity;

// 40 bytes: particles move analytically from their spawn state.
struct Particle
{
    core::Vec3 pos;
    core::Vec3 velocity;
    float time; // < 0 when dead
    float lifetime;
    float size;
    float random;
};

// Cumulative area distribution of a mesh for uniform surface sampling.
class MeshCDF
{
  public:
    explicit MeshCDF(Mesh* mesh);
    int pickRandomTriangle() const;
    void pickRandomPoint(int triangle, float& u, float& v, float& w) const;
    Mesh* getMesh() const
    {
        return m_mesh.get();
    }

  private:
    core::SharedPtr<Mesh> m_mesh;
    core::Array<float> m_cdf;
};

// 0xc8 bytes
class ParticleEmitter
{
  public:
    enum EmitterShape
    {
        BoxShape = 0,
        MeshShape = 1
    };
    enum BlendMode
    {
        TranslucentBlend = 0,
        AdditiveBlend = 1
    };
    // 0x004a9570
    ParticleEmitter();
    virtual ~ParticleEmitter();
    // 0x004a9650 (inner): mesh emitter for the mesh shape when the entity has a mesh
    void emitParticles(ParticleEntity& entity, Particle** particles, int count);
    // 0x004a7e30
    void sprayEmitter(ParticleEntity& entity, Particle** particles, int count);
    // 0x004a8220
    void meshEmitter(ParticleEntity& entity, Particle** particles, int count);

    int m_emitterShape;
    float m_emissionRate;
    float m_emissionTime;
    int m_maxParticles;
    bool m_spawnBurst;
    core::Vec3 m_boxMin;
    core::Vec3 m_boxMax;
    float m_velocityMin, m_velocityMax;
    float m_sprayAngleMin, m_sprayAngleMax;
    core::SharedPtr<RenderableTexture> m_texture;
    float m_frameRate;
    int m_frameSize;
    int m_frameCount;
    float m_lifetimeMin, m_lifetimeMax;
    core::Vec3 m_color[4];
    bool m_colorAnimation;
    float m_opacity;
    float m_fadeIn, m_fadeOut;
    float m_sizeMin, m_sizeMax;
    core::Vec3 m_gravity;
    float m_airResistance;
    float m_rotationSpeed;
    bool m_randomInitialRotation;
    int m_blendMode;
    bool m_objectSpace;
    bool m_clampToGroundPlane;
    float m_depthBias;
};

// 0x30 bytes
class ParticleState
{
  public:
    explicit ParticleState(ParticleEntity* entity);
    ~ParticleState();
    // 0x004a9130
    void setEmitter(ParticleEmitter* emitter);
    Particle* allocParticle();
    // 0x004a9650: with skipEmission the emitted count is advanced without spawning
    void emitParticles(float dt, bool skipEmission);
    // 0x004a8ea0
    void updateParticles(float dt);
    void reset();
    bool isAlive() const;
    ParticleEmitter* getEmitter() const
    {
        return m_emitter.get();
    }
    core::Array<Particle>& getParticles()
    {
        return m_particles;
    }
    int getNumAlive() const
    {
        return m_numAlive;
    }
    float getTime() const
    {
        return m_time;
    }

  private:
    ParticleEntity* m_pEntity;
    core::SharedPtr<ParticleEmitter> m_emitter;
    core::Array<Particle> m_particles;
    core::Array<int> m_freeParticles;
    int m_numEmitted;
    int m_numAlive;
    float m_time;
};

// 0x18 bytes: the emitters and the time stamp of the last edit.
class ParticleSystem
{
  public:
    // 0x004a9920
    ParticleSystem();
    ~ParticleSystem();
    // 0x004a99d0
    void addParticleEmitter(ParticleEmitter* emitter);
    // 0x004a9ac0
    void removeParticleEmitter(ParticleEmitter* emitter);
    // 0x004a9980
    void clear();
    // 0x004a7690: marks the system edited so the entities rebuild their states
    void updateTimeStamp();
    const core::Array<core::SharedPtr<ParticleEmitter>>& getEmitters() const
    {
        return m_emitters;
    }
    long long getTimeStamp() const
    {
        return m_timeStamp;
    }

  private:
    core::Array<core::SharedPtr<ParticleEmitter>> m_emitters;
    long long m_timeStamp;
};

// 0x90 bytes
class ParticleEntity : public RenderEntity
{
  public:
    // 0x004aa2e0
    explicit ParticleEntity(ParticleSystem* system);
    // 0x004a9fe0
    ~ParticleEntity();
    core::AABox3 getWorldBounds() const;
    // 0x004aa110
    void setParticleSystem(ParticleSystem* system);
    ParticleSystem* getParticleSystem() const
    {
        return m_particleSystem.get();
    }
    // 0x004a9b40 / 0x004aa250
    void setMesh(MeshCDF* mesh);
    MeshCDF* getMesh() const
    {
        return m_mesh.get();
    }
    void setSkeleton(Skeleton* skeleton);
    Skeleton* getSkeleton() const
    {
        return m_skeleton.get();
    }
    void setHeightmap(RenderableTexture* texture);
    RenderableTexture* getHeightmap() const
    {
        return m_heightmap.get();
    }
    float getGroundPlaneY() const
    {
        return m_groundPlaneY;
    }
    void setGroundPlaneY(float y)
    {
        m_groundPlaneY = y;
    }
    float getOpacity() const
    {
        return m_opacity;
    }
    void setOpacity(float opacity)
    {
        m_opacity = opacity;
    }
    float getDistanceFadeStart() const
    {
        return m_distanceFadeStart;
    }
    void setDistanceFadeStart(float d)
    {
        m_distanceFadeStart = d;
    }
    float getDistanceFadeEnd() const
    {
        return m_distanceFadeEnd;
    }
    void setDistanceFadeEnd(float d)
    {
        m_distanceFadeEnd = d;
    }
    void start()
    {
        m_emitting = true;
    }
    void stop()
    {
        m_emitting = false;
    }
    // 0x004a8f30
    void reset();
    // 0x004aa260: with skipEmission the particles age without new ones spawning and the
    // bounds are left alone
    void update(float dt, bool skipEmission);
    // 0x004a76f0
    bool isAlive() const;
    const core::Array<ParticleState*>& getStates() const
    {
        return m_states;
    }
    int getRandomSeed() const
    {
        return m_seed;
    }

  private:
    // 0x004a7840
    void updateBounds();
    core::SharedPtr<ParticleSystem> m_particleSystem;
    core::Array<ParticleState*> m_states;
    long long m_timeStamp; // of the system when the states were built
    bool m_emitting;
    core::AABox3 m_bounds;
    int m_seed;
    core::SharedPtr<MeshCDF> m_mesh;
    core::SharedPtr<Skeleton> m_skeleton;
    core::SharedPtr<RenderableTexture> m_heightmap;
    float m_groundPlaneY;
    float m_opacity;
    float m_distanceFadeStart;
    float m_distanceFadeEnd;
};

// Analytic particle position at its current age.
core::Vec3 evaluateParticlePosition(const Particle& p, const ParticleEmitter& e);

} // namespace engine
