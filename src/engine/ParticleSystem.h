// Particle systems, reconstructed from ParticleSystem.cpp (0x08107a50-0x0810c5b0).
#pragma once
#include "core/Array.h"
#include "core/Prim.h"
#include "core/SharedPtr.h"
#include "core/String.h"
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

class ParticleEmitter
{
  public:
    enum BlendMode
    {
        TranslucentBlend = 0,
        AdditiveBlend = 1
    };
    struct Property
    {
        const char* name;
        int type;
        int offset;
        const char* enumNames;
    };
    ParticleEmitter();
    virtual ~ParticleEmitter();
    void emitParticles(ParticleEntity& entity, Particle** particles, int count);
    void sprayEmitter(ParticleEntity& entity, Particle** particles, int count);
    void meshEmitter(ParticleEntity& entity, Particle** particles, int count);
    void setMesh(MeshCDF* mesh);
    void setSkeleton(Skeleton* skeleton);

    float m_emissionRate;
    float m_emissionTime;
    int m_maxParticles;
    bool m_spawnBurst;
    core::Vec3 m_boxMin;
    core::Vec3 m_boxMax;
    float m_velocityMin, m_velocityMax;
    float m_sprayAngleMin, m_sprayAngleMax;
    core::SharedPtr<MeshCDF> m_mesh;
    core::SharedPtr<Skeleton> m_skeleton;
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
    int m_blendMode;
    bool m_objectSpace;
    bool m_clampToGroundPlane;
    float m_depthBias;
    static Property Properties[30];
};

class ParticleState
{
  public:
    explicit ParticleState(ParticleEntity* entity);
    ~ParticleState();
    void setEmitter(ParticleEmitter* emitter);
    Particle* allocParticle();
    void emitParticles(float dt);
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

class ParticleSystem
{
  public:
    ParticleSystem();
    ~ParticleSystem();
    void addParticleEmitter(ParticleEmitter* emitter);
    void removeParticleEmitter(ParticleEmitter* emitter);
    void load(const char* filename);
    const core::Array<core::SharedPtr<ParticleEmitter>>& getEmitters() const
    {
        return m_emitters;
    }
    const core::String& getFilename() const
    {
        return m_filename;
    }
    static ParticleSystem* getParticleSystemByFilename(const char* filename);

  private:
    core::Array<core::SharedPtr<ParticleEmitter>> m_emitters;
    core::String m_filename;
    static core::Array<ParticleSystem*> sm_particleSystems;
};

ParticleSystem* loadParticleSystem(const char* filename);

class ParticleEntity : public RenderEntity
{
  public:
    explicit ParticleEntity(ParticleSystem* system);
    ~ParticleEntity();
    core::AABox3 getWorldBounds();
    void setParticleSystem(ParticleSystem* system);
    ParticleSystem* getParticleSystem() const
    {
        return m_particleSystem.get();
    }
    void start()
    {
        m_emitting = true;
    }
    void stop()
    {
        m_emitting = false;
    }
    void reset();
    void update(float dt);
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
    void updateBounds();
    core::SharedPtr<ParticleSystem> m_particleSystem;
    core::Array<ParticleState*> m_states;
    const void* m_emitterListData;
    int m_emitterListSize;
    bool m_emitting;
    core::AABox3 m_bounds;
    int m_seed;
};

// Analytic particle position at its current age.
core::Vec3 evaluateParticlePosition(const Particle& p, const ParticleEmitter& e);

} // namespace engine
