/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIImage.hpp>

#include <rendering/SafeDeleter.hpp>

#include <rendering/Texture.hpp>

#include <engine/EngineDriver.hpp>

namespace hyperion {

UIImage::UIImage()
{
    SetBackgroundColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
}

void UIImage::Init()
{
    UIObject::Init();
}

void UIImage::SetTexture(const Handle<Texture>& texture)
{
    if (texture == m_texture)
    {
        return;
    }

    m_texture = texture;

    InitObject(m_texture);

    UpdateMaterial(false);
}

MaterialAttributes UIImage::GetMaterialAttributes() const
{
    return UIObject::GetMaterialAttributes();
}

Material::TextureSet UIImage::GetMaterialTextures() const
{
    return Material::TextureSet {
        { MaterialTextureKey::ALBEDO_MAP, m_texture }
    };
}

} // namespace hyperion
