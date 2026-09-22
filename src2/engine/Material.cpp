// Reconstructed from grimrock2.exe Material.cpp.
#include "engine/Material.h"
#include "core/Exception.h"
#include "engine/Texture.h"
#include <cstring>

namespace engine
{

using namespace core;

Array<Material*> Material::sm_materials;

// 0x004bbb40: lighting, ambient occlusion and shadow casting are on by default.
Material::Material(const char* name)
    : m_name(name), m_flags(FlagLighting | FlagAmbientOcclusion | FlagCastShadow),
      m_textureAddressMode(Wrap), m_texcoordScaleOffset(1.0f, 1.0f, 0.0f, 0.0f),
      m_glossiness(DefaultGlossiness), m_depthBias(0.0f), m_numParams(0)
{
    m_registryIndex = (unsigned short)sm_materials.size();
    sm_materials.push_back(this);
}
// 0x004bbef0
Material::Material(const Material& other) : m_numParams(0)
{
    *this = other;
    m_registryIndex = (unsigned short)sm_materials.size();
    sm_materials.push_back(this);
}
// 0x004bb970: the last material takes the slot of the destroyed one.
Material::~Material()
{
    Material* last = sm_materials[sm_materials.size() - 1];
    sm_materials[m_registryIndex] = last;
    last->m_registryIndex = m_registryIndex;
    if (sm_materials.size() > 0)
        sm_materials.resize(sm_materials.size() - 1);
}
// 0x004bbc80
Material& Material::operator=(const Material& other)
{
    if (this == &other)
        return *this;
    m_name = other.m_name;
    m_diffuseMap = other.m_diffuseMap;
    m_specularMap = other.m_specularMap;
    m_normalMap = other.m_normalMap;
    m_emissiveMap = other.m_emissiveMap;
    m_shader = other.m_shader;
    m_flags = other.m_flags;
    m_textureAddressMode = other.m_textureAddressMode;
    m_texcoordScaleOffset = other.m_texcoordScaleOffset;
    m_glossiness = other.m_glossiness;
    m_depthBias = other.m_depthBias;
    for (int i = 0; i < MaxParams; ++i)
        m_params[i] = other.m_params[i];
    m_numParams = other.m_numParams;
    return *this;
}
// 0x004bb7f0
Material::ShaderParam* Material::getParam(const char* name)
{
    for (int i = 0; i < m_numParams; ++i)
        if (strcmp(m_params[i].name.c_str(), name) == 0)
            return &m_params[i];
    if (m_numParams >= MaxParams)
        throw Exception("Too many shader parameters");
    ShaderParam* param = &m_params[m_numParams++];
    param->name = name;
    return param;
}
const Material::ShaderParam* Material::findParam(const char* name) const
{
    for (int i = 0; i < m_numParams; ++i)
        if (strcmp(m_params[i].name.c_str(), name) == 0)
            return &m_params[i];
    return 0;
}
// 0x004bb730
void Material::setParam(const char* name, float v)
{
    ShaderParam* param = getParam(name);
    param->type = ParamFloat;
    param->value.x = v;
}
// 0x004bb750
void Material::setParam(const char* name, const Vec2& v)
{
    ShaderParam* param = getParam(name);
    param->type = ParamVec2;
    param->value.x = v.x;
    param->value.y = v.y;
}
// 0x004bb780
void Material::setParam(const char* name, const Vec3& v)
{
    ShaderParam* param = getParam(name);
    param->type = ParamVec3;
    param->value.x = v.x;
    param->value.y = v.y;
    param->value.z = v.z;
}
// 0x004bb7b0
void Material::setParam(const char* name, const Vec4& v)
{
    ShaderParam* param = getParam(name);
    param->type = ParamVec4;
    param->value = v;
}
// 0x004bbb10
void Material::setTexture(const char* name, RenderableTexture* texture)
{
    ShaderParam* param = getParam(name);
    param->type = ParamTexture;
    param->texture.reset(texture);
}
// 0x004bb6f0
void Material::setTextureFilter(const char* name, TextureFilter filter)
{
    getParam(name)->filter = filter;
}
// 0x004bb710
void Material::setTextureAddress(const char* name, AddressMode mode)
{
    getParam(name)->address = mode;
}
// 0x004bbe70
Material* Material::getMaterialByName(const char* name)
{
    Material* material = findMaterialByName(name);
    if (material)
        return material;
    return new Material(name);
}
// 0x004bb670
Material* Material::findMaterialByName(const char* name)
{
    for (int i = 0; i < sm_materials.size(); ++i)
        if (strcmp(sm_materials[i]->m_name.c_str(), name) == 0)
            return sm_materials[i];
    return 0;
}

} // namespace engine
