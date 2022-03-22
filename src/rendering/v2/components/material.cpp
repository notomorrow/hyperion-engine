#include "material.h"

namespace hyperion::v2 {

Material::Material()
    : m_state(MATERIAL_STATE_DIRTY)
{
    
}

Material::~Material()
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