// Materials of Legend of Grimrock 2, from grimrock2.exe Material.cpp (0x004bb670-0x004bc050).
// A material is 0x658 bytes: name, five resources, packed flags, texture coordinate scale
// and offset, glossiness, depth bias and up to 32 named shader parameters; every material
// is listed in a global registry by index.
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
        Translucent = 3,
        Screen = 4,
        AdditiveSrcAlpha = 5,
        PremultipliedAlpha = 6
    };
    enum AddressMode
    {
        Wrap = 0,
        Clamp = 1,
        WrapU_ClampV_WrapW = 2,
        ClampU_WrapV_WrapW = 3,
        WrapU_ClampV_ClampW = 4,
        ClampU_WrapV_ClampW = 5
    };
    // Bits of the packed flag word (0x36); the low byte is the blend mode.
    enum Flags
    {
        FlagDoubleSided = 0x100,
        FlagLighting = 0x200,
        FlagAmbientOcclusion = 0x400,
        FlagCastShadow = 0x800,
        FlagAlphaTest = 0x1000
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
    static constexpr int MaxParams = 32;

    // 0x30 bytes in the original: name, type, texture, filter, address, value.
    struct ShaderParam
    {
        core::String name;
        int type;
        core::SharedPtr<RenderableTexture> texture;
        int filter;
        int address;
        core::Vec4 value;
        ShaderParam() : type(ParamTexture), filter(Linear_MipNearest), address(Wrap) {}
    };

    // 0x004bbb40
    explicit Material(const char* name);
    // 0x004bbef0
    Material(const Material& other);
    // 0x004bb970
    ~Material();
    // 0x004bbc80: everything but the registry index
    Material& operator=(const Material& other);

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
    RenderableTexture* getEmissiveMap() const
    {
        return m_emissiveMap.get();
    }
    void setEmissiveMap(RenderableTexture* t)
    {
        m_emissiveMap.reset(t);
    }
    RenderableShader* getShader() const
    {
        return m_shader.get();
    }
    void setShader(RenderableShader* s)
    {
        m_shader.reset(s);
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
    bool getDoubleSided() const
    {
        return getFlag(FlagDoubleSided);
    }
    void setDoubleSided(bool b)
    {
        setFlag(FlagDoubleSided, b);
    }
    bool getLighting() const
    {
        return getFlag(FlagLighting);
    }
    void setLighting(bool b)
    {
        setFlag(FlagLighting, b);
    }
    bool getAmbientOcclusion() const
    {
        return getFlag(FlagAmbientOcclusion);
    }
    void setAmbientOcclusion(bool b)
    {
        setFlag(FlagAmbientOcclusion, b);
    }
    bool getCastShadow() const
    {
        return getFlag(FlagCastShadow);
    }
    void setCastShadow(bool b)
    {
        setFlag(FlagCastShadow, b);
    }
    bool getAlphaTest() const
    {
        return getFlag(FlagAlphaTest);
    }
    void setAlphaTest(bool b)
    {
        setFlag(FlagAlphaTest, b);
    }
    BlendMode getBlendMode() const
    {
        return (BlendMode)(m_flags & 0xff);
    }
    void setBlendMode(BlendMode m)
    {
        m_flags = (m_flags & 0xff00) | (unsigned short)m;
    }
    const core::Vec4& getTexcoordScaleOffset() const
    {
        return m_texcoordScaleOffset;
    }
    void setTexcoordScaleOffset(const core::Vec4& v)
    {
        m_texcoordScaleOffset = v;
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

    // 0x004bb7f0: returns the named parameter, creating it if needed (max 32).
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

    // 0x004bbe70: existing material or a new one with that name.
    static Material* getMaterialByName(const char* name);
    // 0x004bb670
    static Material* findMaterialByName(const char* name);
    // Every live material (0x0061ce7c).
    // position in the registry, the sort key of the renderers (+0x34)
    int getRegistryIndex() const
    {
        return m_registryIndex;
    }
    static const core::Array<Material*>& getMaterials()
    {
        return sm_materials;
    }

  private:
    core::String m_name;
    core::SharedPtr<RenderableTexture> m_diffuseMap;
    core::SharedPtr<RenderableTexture> m_specularMap;
    core::SharedPtr<RenderableTexture> m_normalMap;
    core::SharedPtr<RenderableTexture> m_emissiveMap;
    core::SharedPtr<RenderableShader> m_shader;
    unsigned short m_registryIndex;
    unsigned short m_flags;
    AddressMode m_textureAddressMode;
    core::Vec4 m_texcoordScaleOffset;
    float m_glossiness;
    float m_depthBias;
    ShaderParam m_params[MaxParams];
    int m_numParams;
    static core::Array<Material*> sm_materials;
};

} // namespace engine
