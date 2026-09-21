// Reconstructed from Grimrock.bin.x86 Material.cpp.
#include "engine/Material.h"
#include "core/Exception.h"
#include "engine/Texture.h"
#include <cstring>

namespace engine
{

using namespace core;

Array<Material*> Material::sm_materials;
// 0x080faff0: the default material is created at start-up.
SharedPtr<Material> Material::Default(new Material("default"));

Material::Property Material::Properties[11] = {
    {"DiffuseMap", 4, 0, 0},
    {"SpecularMap", 4, 0, 0},
    {"NormalMap", 4, 0, 0},
    {"DoubleSided", 2, 0, 0},
    {"Lighting", 2, 0, 0},
    {"AlphaTest", 2, 0, 0},
    {"BlendMode", 3, 0, "Opaque|Additive|Modulative|Translucent"},
    {"TextureAddressMode", 3, 0, "Wrap|Clamp"},
    {"Glossiness", 1, 0, 0},
    {"DepthBias", 1, 0, 0},
    {0, 0, 0, 0}};

// 0x080faa00
Material::Material(const char* name)
    : m_name(name), m_doubleSided(false), m_lighting(true), m_alphaTest(false), m_blendMode(Opaque),
      m_textureAddressMode(Wrap), m_glossiness(DefaultGlossiness), m_depthBias(0.0f), m_numParams(0)
{
    sm_materials.push_back(this);
}
// 0x080fa340
Material::~Material()
{
    sm_materials.remove(this);
}
// 0x080f9fa0
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
// 0x080fa1a0
void Material::setParam(const char* name, float v)
{
    ShaderParam* param = getParam(name);
    param->type = ParamFloat;
    param->value.x = v;
}
// 0x080fa160
void Material::setParam(const char* name, const Vec2& v)
{
    ShaderParam* param = getParam(name);
    param->type = ParamVec2;
    param->value.x = v.x;
    param->value.y = v.y;
}
// 0x080fa120
void Material::setParam(const char* name, const Vec3& v)
{
    ShaderParam* param = getParam(name);
    param->type = ParamVec3;
    param->value.x = v.x;
    param->value.y = v.y;
    param->value.z = v.z;
}
// 0x080fa0e0
void Material::setParam(const char* name, const Vec4& v)
{
    ShaderParam* param = getParam(name);
    param->type = ParamVec4;
    param->value = v;
}
// 0x080fa210
void Material::setTexture(const char* name, RenderableTexture* texture)
{
    ShaderParam* param = getParam(name);
    param->type = ParamTexture;
    param->texture.reset(texture);
}
// 0x080fa1f0
void Material::setTextureFilter(const char* name, TextureFilter filter)
{
    getParam(name)->filter = filter;
}
// 0x080fa1d0
void Material::setTextureAddress(const char* name, AddressMode mode)
{
    getParam(name)->address = mode;
}
// 0x080faf50
Material* Material::getMaterialByName(const char* name)
{
    Material* material = findMaterialByName(name);
    if (material)
        return material;
    return new Material(name);
}
// 0x080f9f30
Material* Material::findMaterialByName(const char* name)
{
    for (int i = 0; i < sm_materials.size(); ++i)
        if (strcmp(sm_materials[i]->m_name.c_str(), name) == 0)
            return sm_materials[i];
    return 0;
}

// ---- MaterialLibrary -------------------------------------------------------------

// 0x080fb690: material files are DataDef documents made of "material" blocks whose
// attributes follow Material::Properties. The game defines its materials from Lua, so
// the text loader is not needed for playing.
void MaterialLibrary::load(const char* filename, int flags)
{
    m_materials.clear();
    throw Exception("Material library files are not supported: %s", filename);
}
// 0x080f9d30
void MaterialLibrary::save(const char* filename)
{
    throw Exception("Material library files are not supported: %s", filename);
}
// 0x080fb330
void MaterialLibrary::addMaterial(Material* material)
{
    m_materials.push_back(SharedPtr<Material>(material));
}
// 0x080fa840
void MaterialLibrary::removeMaterial(Material* material)
{
    for (int i = 0; i < m_materials.size(); ++i)
    {
        if (m_materials[i].get() == material)
        {
            m_materials.erase(i);
            return;
        }
    }
}
// 0x080f9ec0
Material* MaterialLibrary::findMaterial(const char* name) const
{
    for (int i = 0; i < m_materials.size(); ++i)
        if (strcmp(m_materials[i]->getName().c_str(), name) == 0)
            return m_materials[i].get();
    return 0;
}
// 0x080fba00
MaterialLibrary* loadMaterialLibrary(const char* filename, int flags)
{
    MaterialLibrary* lib = new MaterialLibrary;
    lib->load(filename, flags);
    return lib;
}

} // namespace engine
