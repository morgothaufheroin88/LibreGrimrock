// Render entities and skeleton of Legend of Grimrock 2, reconstructed from grimrock2.exe
// RenderEntity.cpp (0x004b3420-0x004b5100).
#pragma once
#include "core/Array.h"
#include "core/Prim.h"
#include "core/SharedPtr.h"
#include "engine/Material.h"
#include "engine/Node.h"

namespace engine
{

class Mesh;
class RenderableMesh;
class RenderableTexture;
class Camera;

// Bones plus the inverse bind matrices; the skinning matrices are cached per frame
// (0x28 bytes).
class Skeleton
{
  public:
    // 0x004b4620
    Skeleton();
    // 0x004b4650
    void addBone(Node& bone, const core::Matrix4x3& invBindMatrix);
    // 0x004b3af0: out[i] = worldToModel * bone[i].localToWorld * invBind[i]
    void computeSkinningMatrices(const core::Matrix4x3& worldToModel, core::Matrix4x3* out) const;
    // 0x004b46b0: the matrices of the entity's node, computed once per frame and packed
    // as 3 vec4 rows per bone into out (12 floats per bone).
    void computeSkinningMatrices(const class MeshEntity& entity, float* out, int frame);
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
    core::Array<core::Matrix4x3> m_skinningMatrices;
    int m_frame;
};

class RenderEntity : public Component
{
  public:
    enum EntityType
    {
        MeshEntityType = 0,
        LightEntityType = 1,
        ParticleEntityType = 2,
        OccluderEntityType = 3
    };
    static constexpr unsigned int DefaultPassMask = 0xffffffff;

    // 0x004b3420
    explicit RenderEntity(EntityType type);
    ~RenderEntity();
    // 0x004b3570 / 0x004b35a0: registers with the scene's spatial data structure
    void addedToNode(Node* node);
    void removedFromNode();
    virtual core::AABox3 getWorldBounds() const = 0;
    // 0x004b3460: draws the world bounds
    virtual void visualize();

    EntityType getEntityType() const
    {
        return m_entityType;
    }
    // Slot in the spatial data structure, -1 when not registered.
    int getSpatialIndex() const
    {
        return m_spatialIndex;
    }
    void setSpatialIndex(int index)
    {
        m_spatialIndex = index;
    }
    // Entities without finite bounds (ambient/directional lights, empty meshes) are
    // returned by every spatial query.
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
    unsigned int getPassMask() const
    {
        return m_passMask;
    }
    void setPassMask(unsigned int mask)
    {
        m_passMask = mask;
    }
    int getRenderHack() const
    {
        return m_renderHack;
    }
    void setRenderHack(int hack)
    {
        m_renderHack = hack;
    }

  protected:
    // 0x004b35c0: re-inserts the entity into the spatial data structure
    void notifyBoundChanged();

    int m_spatialIndex;
    bool m_unbounded;
    EntityType m_entityType;
    bool m_drawBoundBox;
    bool m_hidden;
    float m_sortOffset;
    unsigned int m_passMask;
    int m_renderHack;
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
        ManualBounds = 0x20,     // bounds in node space
        ManualWorldBounds = 0x40 // bounds already in world space (setManualBounds2)
    };
    static constexpr float DefaultShadowMaxDistance = 100000.0f;
    static constexpr float DefaultSkinningDistance = 100000.0f;

    // 0x004b5020
    explicit MeshEntity(RenderableMesh* mesh);
    // 0x004b4ea0
    ~MeshEntity();
    // 0x004b35f0
    core::AABox3 getWorldBounds() const;
    void visualize();

    // 0x004b4f40: adopts the mesh's default materials
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
    // 0x004b4490: every segment
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
    // 0x004b3680
    void setManualBounds(const core::AABox3& bounds, bool worldSpace);
    float getDissolve() const
    {
        return m_dissolve;
    }
    void setDissolve(float f)
    {
        m_dissolve = f;
    }
    float getDissolveStart() const
    {
        return m_dissolveStart;
    }
    void setDissolveStart(float f)
    {
        m_dissolveStart = f;
    }
    float getDissolveEnd() const
    {
        return m_dissolveEnd;
    }
    void setDissolveEnd(float f)
    {
        m_dissolveEnd = f;
    }
    float getShadowMinDistance() const
    {
        return m_shadowMinDistance;
    }
    void setShadowMinDistance(float f)
    {
        m_shadowMinDistance = f;
    }
    float getShadowMaxDistance() const
    {
        return m_shadowMaxDistance;
    }
    void setShadowMaxDistance(float f)
    {
        m_shadowMaxDistance = f;
    }
    float getSkinningDistance() const
    {
        return m_skinningDistance;
    }
    void setSkinningDistance(float f)
    {
        m_skinningDistance = f;
    }
    // Skinning is used this frame (set by the render visitor from the skinning distance).
    bool isSkinned() const
    {
        return m_skinned;
    }
    void setSkinned(bool b)
    {
        m_skinned = b;
    }
    // Squared distance to the camera, scaled by its lod factor (set by the render visitor).
    float getDistance() const
    {
        return m_distance;
    }
    void setDistance(float d)
    {
        m_distance = d;
    }

  private:
    bool m_skinned;
    core::SharedPtr<RenderableMesh> m_mesh;
    core::SharedPtr<Skeleton> m_skeleton;
    core::Array<core::SharedPtr<Material>> m_materials;
    core::Vec3 m_emissiveColor;
    unsigned int m_flags;
    core::AABox3 m_bounds;
    float m_dissolve;
    float m_dissolveStart;
    float m_dissolveEnd;
    float m_shadowMinDistance;
    float m_shadowMaxDistance;
    float m_skinningDistance;
    float m_distance;
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
    static constexpr float DefaultMaxShadowDistance = 100000.0f;
    static constexpr float DefaultClipDistance = 100000.0f;
    static constexpr int DefaultShadowMapSize = 256;
    static constexpr int NumClipDistances = 6; // one per cube face of a point light

    // 0x004b48d0
    explicit LightEntity(LightType type);
    // 0x004b4cd0
    ~LightEntity();
    core::AABox3 getWorldBounds() const;
    // 0x004b3490
    void visualize();

    // 0x004b36f0
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
    const core::Vec3& getLightColor2() const
    {
        return m_color2;
    }
    void setLightColor2(const core::Vec3& c)
    {
        m_color2 = c;
    }
    const core::Vec3& getLightColor3() const
    {
        return m_color3;
    }
    void setLightColor3(const core::Vec3& c)
    {
        m_color3 = c;
    }
    // 0x004b3740
    void setLightRange(float range);
    float getLightRange() const
    {
        return m_range;
    }
    // 0x004b3780
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
    // 0x004b37c0: how far the light reaches through the given cube face (point lights)
    // or in general (face 0), used to shrink the shadow frusta.
    void setClipDistance(int face, float distance);
    float getClipDistance(int face) const
    {
        return m_clipDistances[face];
    }
    // 0x004b4100: bit i set when cube face i of a point light shows in the camera.
    unsigned int getVisibleFaceMask(const Camera& camera) const;
    // Faces left after the camera and occlusion tests of the frame (+0x28), set by the
    // renderer before the light pass.
    unsigned int getVisibleFaces() const
    {
        return m_visibleFaces;
    }
    void setVisibleFaces(unsigned int mask)
    {
        m_visibleFaces = mask;
    }
    void setStaticShadowMap(RenderableTexture* texture);
    RenderableTexture* getStaticShadowMap() const
    {
        return m_staticShadowMap.get();
    }
    // 0x004b4a10: renders through the active renderer; with updateClipDistances the
    // measured extents replace the positive clip distances.
    RenderableTexture* renderStaticShadowMap(int size, float bias, int flags,
                                             bool updateClipDistances);
    bool getPrimaryLight() const
    {
        return m_primaryLight;
    }
    void setPrimaryLight(bool b)
    {
        m_primaryLight = b;
    }
    bool getSpecular() const
    {
        return m_specular;
    }
    void setSpecular(bool b)
    {
        m_specular = b;
    }
    bool getDebugDraw() const
    {
        return m_debugDraw;
    }
    void setDebugDraw(bool b)
    {
        m_debugDraw = b;
    }

  private:
    unsigned int m_visibleFaces;
    LightType m_lightType;
    core::Vec3 m_color;
    core::Vec3 m_color2;
    core::Vec3 m_color3;
    float m_range;
    float m_spotAngle;
    float m_spotSharpness;
    bool m_castShadow;
    float m_maxShadowDistance;
    int m_shadowMapSize;
    float m_clipDistances[NumClipDistances];
    core::SharedPtr<RenderableTexture> m_staticShadowMap;
    bool m_primaryLight;
    bool m_specular;
    bool m_debugDraw;
};

// Occluder geometry for the software occlusion culling (0x004b4d70).
class OccluderEntity : public RenderEntity
{
  public:
    explicit OccluderEntity(Mesh* mesh);
    // 0x004b4e00
    ~OccluderEntity();
    // 0x004b3800
    core::AABox3 getWorldBounds() const;
    // 0x004b4af0
    void setMesh(Mesh* mesh);
    Mesh* getMesh() const
    {
        return m_mesh.get();
    }
    float getRadius() const
    {
        return m_radius;
    }

  private:
    core::SharedPtr<Mesh> m_mesh;
    float m_radius; // of the bounding sphere around the origin
};

} // namespace engine
