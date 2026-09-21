// Materials and material libraries, from Material.cpp (0x080f9d30-0x080fba00).
#pragma once
#include "core/Array.h"
#include "core/SharedPtr.h"
#include "core/String.h"
#include "core/Vector.h"
#include "engine/Renderer.h"
#include "engine/Texture.h"

namespace engine
{

class Material
{
  public:
    static constexpr float DefaultGlossiness = 20.0f;
    enum BlendMode
    {
        Opaque = 0,
        Additive = 1,
        Modulative = 2,
        Translucent = 3
    };
    enum AddressMode
    {
        Wrap = 0,
        Clamp = 1
    };
    enum TextureFilter
    {
        Nearest = 0,
        Linear = 1,
        Nearest_MipNearest = 2,
        Linear_MipNearest = 3,
        Linear_MipLinear = 4,
        Anisotropic = 5
    };
    enum ParamType
    {
        ParamTexture = 0,
        ParamFloat = 1,
        ParamVec2 = 2,
        ParamVec3 = 3,
        ParamVec4 = 4
    };
    static constexpr int MaxParams = 16;

    // 0x30 bytes in the original: name, type, texture, filter, address, value.
    struct ShaderParam
    {
        core::String name;
        int type;
        core::SharedPtr<RenderableTexture> texture;
        int filter;
        int address;
        core::Vec4 value;
        ShaderParam() : type(ParamFloat), filter(Linear_MipNearest), address(Wrap) {}
    };
    // Property table used by material files (name, type, member offset, enum names).
    struct Property
    {
        const char* name;
        int type; // 0 int, 1 float, 2 bool, 3 enum, 4 texture
        int offset;
        const char* enumNames;
    };

    explicit Material(const char* name);
    ~Material();

    const core::String& getName() const
    {
        return m_name;
    }
    void setName(const char* name)
    {
        m_name = name;
    }
    RenderableTexture* getDiffuseMap() const
    {
        return m_diffuseMap.get();
    }
    void setDiffuseMap(RenderableTexture* t)
    {
        m_diffuseMap.reset(t);
    }
    RenderableTexture* getSpecularMap() const
    {
        return m_specularMap.get();
    }
    void setSpecularMap(RenderableTexture* t)
    {
        m_specularMap.reset(t);
    }
    RenderableTexture* getNormalMap() const
    {
        return m_normalMap.get();
    }
    void setNormalMap(RenderableTexture* t)
    {
        m_normalMap.reset(t);
    }
    RenderableShader* getShader() const
    {
        return m_shader.get();
    }
    void setShader(RenderableShader* s)
    {
        m_shader.reset(s);
    }
    bool getDoubleSided() const
    {
        return m_doubleSided;
    }
    void setDoubleSided(bool b)
    {
        m_doubleSided = b;
    }
    bool getLighting() const
    {
        return m_lighting;
    }
    void setLighting(bool b)
    {
        m_lighting = b;
    }
    bool getAlphaTest() const
    {
        return m_alphaTest;
    }
    void setAlphaTest(bool b)
    {
        m_alphaTest = b;
    }
    BlendMode getBlendMode() const
    {
        return m_blendMode;
    }
    void setBlendMode(BlendMode m)
    {
        m_blendMode = m;
    }
    AddressMode getTextureAddressMode() const
    {
        return m_textureAddressMode;
    }
    void setTextureAddressMode(AddressMode m)
    {
        m_textureAddressMode = m;
    }
    float getGlossiness() const
    {
        return m_glossiness;
    }
    void setGlossiness(float g)
    {
        m_glossiness = g;
    }
    float getDepthBias() const
    {
        return m_depthBias;
    }
    void setDepthBias(float d)
    {
        m_depthBias = d;
    }

    // 0x080f9fa0: returns the named parameter, creating it if needed (max 16).
    ShaderParam* getParam(const char* name);
    const ShaderParam* findParam(const char* name) const;
    void setParam(const char* name, float v);
    void setParam(const char* name, const core::Vec2& v);
    void setParam(const char* name, const core::Vec3& v);
    void setParam(const char* name, const core::Vec4& v);
    void setTexture(const char* name, RenderableTexture* texture);
    void setTextureFilter(const char* name, TextureFilter filter);
    void setTextureAddress(const char* name, AddressMode mode);
    int getParamCount() const
    {
        return m_numParams;
    }
    const ShaderParam& getParam(int i) const
    {
        return m_params[i];
    }

    // 0x080faf50: existing material or a new one with that name.
    static Material* getMaterialByName(const char* name);
    // 0x080f9f30
    static Material* findMaterialByName(const char* name);
    static core::SharedPtr<Material> Default;
    static Property Properties[11];
    static const core::Array<Material*>& getMaterials()
    {
        return sm_materials;
    }

  private:
    core::String m_name;
    core::SharedPtr<RenderableTexture> m_diffuseMap;
    core::SharedPtr<RenderableTexture> m_specularMap;
    core::SharedPtr<RenderableTexture> m_normalMap;
    core::SharedPtr<RenderableShader> m_shader;
    bool m_doubleSided;
    bool m_lighting;
    bool m_alphaTest;
    BlendMode m_blendMode;
    AddressMode m_textureAddressMode;
    float m_glossiness;
    float m_depthBias;
    ShaderParam m_params[MaxParams];
    int m_numParams;
    static core::Array<Material*> sm_materials;
};

class MaterialLibrary
{
  public:
    void load(const char* filename, int flags);
    void save(const char* filename);
    void addMaterial(Material* material);
    void removeMaterial(Material* material);
    Material* findMaterial(const char* name) const;
    int getMaterialCount() const
    {
        return m_materials.size();
    }
    Material* getMaterial(int i) const
    {
        return m_materials[i].get();
    }

  private:
    core::Array<core::SharedPtr<Material>> m_materials;
};

MaterialLibrary* loadMaterialLibrary(const char* filename, int flags);

} // namespace engine
