/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/lightmapper/LightmapVolume.hpp>
#include <scene/Texture.hpp>

#include <rendering/RenderTexture.hpp>
#include <rendering/lightmapper/RenderLightmapVolume.hpp>
#include <rendering/lightmapper/LightmapUVBuilder.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/threading/Threads.hpp>

#include <core/algorithm/AnyOf.hpp>

#include <Engine.hpp>

namespace hyperion {

LightmapVolume::LightmapVolume()
    : m_render_resource(nullptr),
      m_aabb(BoundingBox::Empty()),
      m_atlas(Vec2u(4096, 4096))
{
}

LightmapVolume::LightmapVolume(const BoundingBox& aabb)
    : m_render_resource(nullptr),
      m_aabb(aabb),
      m_atlas(Vec2u(4096, 4096))
{
}

LightmapVolume::~LightmapVolume()
{
    if (IsInitCalled())
    {
        SetReady(false);

        if (m_render_resource != nullptr)
        {
            FreeResource(m_render_resource);
        }
    }
}

bool LightmapVolume::AddElement(const LightmapUVMap& uv_map, LightmapElement& out_element)
{
    const Vec2u element_dimensions = { uv_map.width, uv_map.height };

    if (!m_atlas.AddElement(element_dimensions, out_element))
    {
        return false;
    }

    FixedArray<Bitmap<4, float>, uint32(LightmapElementTextureType::MAX)> bitmaps = {
        uv_map.ToBitmapRadiance(),  /* RADIANCE */
        uv_map.ToBitmapIrradiance() /* IRRADIANCE */
    };

    out_element.entries.Resize(uint32(LightmapElementTextureType::MAX));

    for (uint32 i = 0; i < uint32(LightmapElementTextureType::MAX); i++)
    {
        out_element.entries[i].type = LightmapElementTextureType(i);

        Handle<Texture> texture = CreateObject<Texture>(TextureData {
            TextureDesc {
                ImageType::TEXTURE_TYPE_2D,
                InternalFormat::RGBA32F,
                Vec3u { element_dimensions, 1 },
                FilterMode::TEXTURE_FILTER_LINEAR,
                FilterMode::TEXTURE_FILTER_LINEAR,
                WrapMode::TEXTURE_WRAP_REPEAT },
            ByteBuffer(bitmaps[i].GetUnpackedFloats().ToByteView()) });

        out_element.entries[i].texture = std::move(texture);
    }

    out_element.index = m_elements.Size();

    m_elements.PushBack(out_element);

    if (IsInitCalled())
    {
        // Initalize textures for the entries in the element
        for (LightmapElementTextureEntry& entry : out_element.entries)
        {
            if (entry.texture.IsValid())
            {
                InitObject(entry.texture);
            }
        }

        UpdateAtlasTextures();
    }

    return true;
}

const LightmapElement* LightmapVolume::GetElement(uint32 index) const
{
    Threads::AssertOnThread(g_game_thread);

    if (index >= m_elements.Size())
    {
        return nullptr;
    }

    return &m_elements[index];
}

void LightmapVolume::Init()
{
    if (IsInitCalled())
    {
        return;
    }

    HypObject::Init();

    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind(
        [this]()
        {
            if (m_render_resource != nullptr)
            {
                FreeResource(m_render_resource);

                m_render_resource = nullptr;
            }
        }));

    m_render_resource = AllocateResource<RenderLightmapVolume>(this);

    for (LightmapElement& element : m_elements)
    {
        if (!element.IsValid())
        {
            continue;
        }

        for (LightmapElementTextureEntry& entry : element.entries)
        {
            InitObject(entry.texture);
        }
    }

    UpdateAtlasTextures();

    SetReady(true);
}

void LightmapVolume::UpdateAtlasTextures()
{
    HYP_LOG(Lightmap, Debug, "Updating atlas textures for LightmapVolume {}", m_uuid);

    Array<TResourceHandle<RenderTexture>> atlas_textures;
    atlas_textures.Resize(uint32(LightmapElementTextureType::MAX));

    m_atlas_textures.Resize(uint32(LightmapElementTextureType::MAX));

    for (SizeType i = 0; i < m_atlas_textures.Size(); i++)
    {
        if (!m_atlas_textures[i].IsValid())
        {
            m_atlas_textures[i] = CreateObject<Texture>(TextureDesc {
                ImageType::TEXTURE_TYPE_2D,
                InternalFormat::RGBA8,
                Vec3u { m_atlas.atlas_dimensions, 1 },
                FilterMode::TEXTURE_FILTER_LINEAR,
                FilterMode::TEXTURE_FILTER_LINEAR,
                WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE });
        }

        InitObject(m_atlas_textures[i]);

        atlas_textures[i] = TResourceHandle<RenderTexture>(m_atlas_textures[i]->GetRenderResource());
    }

    if (IsInitCalled())
    {
        m_render_resource->BuildAtlasTextures(std::move(atlas_textures), m_elements);
    }
}

} // namespace hyperion
