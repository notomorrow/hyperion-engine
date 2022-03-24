#include "material.h"

namespace hyperion::v2 {

Material::Material()
    : m_state(MATERIAL_STATE_DIRTY)
{
    
}

Material::Material(const Material &other)
    : m_parameters(other.m_parameters),
      m_state(other.m_state)
{
}

Material &Material::operator=(const Material &other) = default;

Material::Material(Material &&other)
    : m_parameters(std::move(other.m_parameters)),
      m_state(other.m_state)
{
    other.m_state = MATERIAL_STATE_DIRTY;
}

Material &Material::operator=(Material &&other)
{
    m_parameters = std::move(other.m_parameters);
    m_state = other.m_state;
    other.m_state = MATERIAL_STATE_DIRTY;

    return *this;
}

Material::~Material() = default;

void Material::Create(Engine *engine, MaterialBuffer *material_buffer)
{
    material_buffer->m_material_data.push_back(MaterialData{
        .albedo = GetParameter<Vector4>(Material::MATERIAL_KEY_ALBEDO),
        .metalness = GetParameter<float>(Material::MATERIAL_KEY_METALNESS),
        .roughness = GetParameter<float>(Material::MATERIAL_KEY_ROUGHNESS)
    });
}

void Material::Destroy(Engine *engine)
{
    
}

void Material::SetParameter(MaterialKey key, const Parameter &value)
{
    m_parameters.Set(key, value);

    m_state = MATERIAL_STATE_DIRTY;
}

void Material::SetTexture(TextureSet::TextureKey key, Texture::ID id)
{
    //m_textures.Set(key, id);

    m_state = MATERIAL_STATE_DIRTY;
}

} // namespace hyperion::v2