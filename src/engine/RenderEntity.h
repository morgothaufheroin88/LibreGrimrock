// Render entities and skeleton, reconstructed from RenderEntity.cpp (0x080ec990-0x080ef530).
#pragma once
#include "core/Array.h"
#include "core/Prim.h"
#include "core/SharedPtr.h"
#include "engine/Material.h"
#include "engine/Node.h"

namespace engine
{

class RenderableMesh;
class RenderableTexture;
class SpatialNode;

class Skeleton
{
  public:
    // 0x080ece90
    void addBone(Node& bone, const core::Matrix4x3& invBindMatrix);
    // 0x080ed0e0: out[i] = worldToModel * bone[i].localToWorld * invBind[i]
    void computeSkinningMatrices(const core::Matrix4x3& worldToModel, core::Matrix4x3* out) const;
    // 0x080ecde0
    void drawRestPose();
    int getBoneCount() const
    {
        return m_bones.size();
    }
    Node* getBone(int i) const
    {
        return m_bones[i];
    }
    const core::Matrix4x3& getInvBindMatrix(int i) const
    {
        return m_invBindMatrices[i];
    }

  private:
    core::Array<Node*> m_bones;
    core::Array<core::Matrix4x3> m_invBindMatrices;
};

class RenderEntity : public Component
{
  public:
    enum EntityType
    {
        MeshEntityType = 0,
        LightEntityType = 1,
        ParticleEntityType = 2
    };

    explicit RenderEntity(EntityType type);
    ~RenderEntity();
    void addedToNode(Node* node);
    void removedFromNode();
    virtual core::AABox3 getWorldBounds() = 0;
    virtual void visualize();

    EntityType getEntityType() const
    {
        return m_entityType;
    }
    SpatialNode* getSpatialNode() const
    {
        return m_pSpatialNode;
    }
    // Entities without finite bounds (ambient/directional lights, empty meshes) are
    // visited by every spatial query.
    bool isUnbounded() const
    {
        return m_unbounded;
    }
    bool getDrawBoundBox() const
    {
        return m_drawBoundBox;
    }
    void setDrawBoundBox(bool b)
    {
        m_drawBoundBox = b;
    }
    bool getHidden() const
    {
        return m_hidden;
    }
    void setHidden(bool b)
    {
        m_hidden = b;
    }
    float getSortOffset() const
    {
        return m_sortOffset;
    }
    void setSortOffset(float f)
    {
        m_sortOffset = f;
    }

  protected:
    bool m_unbounded;
    SpatialNode* m_pSpatialNode;
    EntityType m_entityType;
    bool m_drawBoundBox;
    bool m_hidden;
    float m_sortOffset;
};

class MeshEntity : public RenderEntity
{
  public:
    enum Flags
    {
        CastShadow = 1,
        StaticShadow = 2,
        DrawNormals = 4,
        DrawTangents = 8,
        ManualBounds = 0x20
    };
    explicit MeshEntity(RenderableMesh* mesh);
    ~MeshEntity();
    core::AABox3 getWorldBounds();
    void visualize();

    void setMesh(RenderableMesh* mesh);
    RenderableMesh* getMesh() const
    {
        return m_mesh.get();
    }
    void setSkeleton(Skeleton* skeleton);
    Skeleton* getSkeleton() const
    {
        return m_skeleton.get();
    }
    void setMaterial(Material* material);
    void setMaterials(const core::Array<core::SharedPtr<Material>>& materials);
    const core::Array<core::SharedPtr<Material>>& getMaterials() const
    {
        return m_materials;
    }
    core::Array<core::SharedPtr<Material>>& getMaterials()
    {
        return m_materials;
    }
    const core::Vec3& getEmissiveColor() const
    {
        return m_emissiveColor;
    }
    void setEmissiveColor(const core::Vec3& c)
    {
        m_emissiveColor = c;
    }
    bool getFlag(int flag) const
    {
        return (m_flags & flag) != 0;
    }
    void setFlag(int flag, bool on)
    {
        if (on)
            m_flags |= flag;
        else
            m_flags &= ~flag;
    }
    unsigned int getFlags() const
    {
        return m_flags;
    }
    void setManualBounds(const core::AABox3& bounds)
    {
        m_flags |= ManualBounds;
        m_bounds = bounds;
    }

  private:
    core::SharedPtr<RenderableMesh> m_mesh;
    core::SharedPtr<Skeleton> m_skeleton;
    core::Array<core::SharedPtr<Material>> m_materials;
    core::Vec3 m_emissiveColor;
    unsigned int m_flags;
    core::AABox3 m_bounds;
};

class LightEntity : public RenderEntity
{
  public:
    enum LightType
    {
        Ambient = 0,
        Directional = 1,
        Point = 2,
        Spot = 3
    };
    explicit LightEntity(LightType type);
    ~LightEntity();
    core::AABox3 getWorldBounds();
    void visualize();

    void setLightType(LightType type);
    LightType getLightType() const
    {
        return m_lightType;
    }
    const core::Vec3& getLightColor() const
    {
        return m_color;
    }
    void setLightColor(const core::Vec3& c)
    {
        m_color = c;
    }
    void setLightRange(float range);
    float getLightRange() const
    {
        return m_range;
    }
    void setSpotAngle(float angle);
    float getSpotAngle() const
    {
        return m_spotAngle;
    }
    void setSpotSharpness(float s)
    {
        m_spotSharpness = s;
    }
    float getSpotSharpness() const
    {
        return m_spotSharpness;
    }
    bool getCastShadow() const
    {
        return m_castShadow;
    }
    void setCastShadow(bool b)
    {
        m_castShadow = b;
    }
    float getMaxShadowDistance() const
    {
        return m_maxShadowDistance;
    }
    void setMaxShadowDistance(float d)
    {
        m_maxShadowDistance = d;
    }
    int getShadowMapSize() const
    {
        return m_shadowMapSize;
    }
    void setShadowMapSize(int size)
    {
        m_shadowMapSize = size;
    }
    void setStaticShadowMap(RenderableTexture* texture);
    RenderableTexture* getStaticShadowMap() const
    {
        return m_staticShadowMap.get();
    }
    // 0x080ecae0
    RenderableTexture* renderStaticShadowMap(int size, float bias, int flags);
    bool getPrimaryLight() const
    {
        return m_primaryLight;
    }
    void setPrimaryLight(bool b)
    {
        m_primaryLight = b;
    }

  private:
    LightType m_lightType;
    core::Vec3 m_color;
    float m_range;
    float m_spotAngle;
    float m_spotSharpness;
    bool m_castShadow;
    float m_maxShadowDistance;
    int m_shadowMapSize;
    core::SharedPtr<RenderableTexture> m_staticShadowMap;
    bool m_primaryLight;
};

} // namespace engine
