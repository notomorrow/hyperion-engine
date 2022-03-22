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