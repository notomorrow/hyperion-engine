/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/AssetRegistry.hpp>

#include <rendering/RenderStructs.hpp>

namespace hyperion {

HYP_CLASS()
class TextureAsset : public AssetObject
{
    HYP_OBJECT_BODY(TextureAsset);

public:
    TextureAsset() = default;

    TextureAsset(Name name, const TextureData& textureData)
        : AssetObject(name, textureData),
          m_textureDesc(textureData.desc)
    {
    }

    HYP_FORCE_INLINE const TextureDesc& GetTextureDesc() const
    {
        return m_textureDesc;
    }

    HYP_FORCE_INLINE TextureData* GetTextureData() const
    {
        return GetResourceData<TextureData>();
    }

private:
    TextureDesc m_textureDesc;
};

} // namespace hyperion
