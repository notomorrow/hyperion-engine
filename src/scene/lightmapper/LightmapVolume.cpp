/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/lightmapper/LightmapVolume.hpp>
#include <scene/Texture.hpp>

#include <rendering/RenderTexture.hpp>
#include <rendering/RenderProxy.hpp>

#include <rendering/rhi/CmdList.hpp>

#include <rendering/backend/RenderCommand.hpp>
#include <rendering/backend/RendererHelpers.hpp>

#include <rendering/lightmapper/RenderLightmapVolume.hpp>
#include <rendering/lightmapper/LightmapUVBuilder.hpp>

#include <core/io/ByteWriter.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/threading/Threads.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(BakeLightmapVolumeTexture)
    : RenderCommand
{
    WeakHandle<LightmapVolume> lightmapVolumeWeak;
    Array<LightmapElement> lightmapElements;
    Array<Handle<Texture>> atlasTextures;

    RENDER_COMMAND(BakeLightmapVolumeTexture)(const WeakHandle<LightmapVolume>& lightmapVolumeWeak, const Array<LightmapElement>& lightmapElements, const Array<Handle<Texture>>& atlasTextures)
        : lightmapVolumeWeak(lightmapVolumeWeak),
          lightmapElements(lightmapElements),
          atlasTextures(atlasTextures)
    {
    }

    virtual ~RENDER_COMMAND(BakeLightmapVolumeTexture)() override
    {
    }

    virtual RendererResult operator()() override
    {
        // Ensure the array of atlas textures are resized to the correct count
        Assert(atlasTextures.Size() == uint32(LTT_MAX));

        Array<Array<Pair<const LightmapElement*, Handle<Texture>>>> elementTextures;
        elementTextures.Resize(uint32(LTT_MAX));

        for (const LightmapElement& element : lightmapElements)
        {
            for (const LightmapElementTextureEntry& entry : element.entries)
            {
                if (!atlasTextures[uint32(entry.type)])
                {
                    HYP_LOG(Lightmap, Warning, "Atlas texture for type {} is not set, skipping element {}", uint32(entry.type), element.index);

                    continue;
                }

                if (!entry.texture.IsValid())
                {
                    continue;
                }

                Assert(entry.texture->IsReady());

                elementTextures[uint32(entry.type)].EmplaceBack(&element, entry.texture);
            }
        }

        SingleTimeCommands commands;

        commands.Push([&](CmdList& cmd)
            {
                for (uint32 textureTypeIndex = 0; textureTypeIndex < uint32(LTT_MAX); textureTypeIndex++)
                {
                    if (!atlasTextures[textureTypeIndex])
                    {
                        continue;
                    }

                    const Handle<Texture>& atlasTexture = atlasTextures[textureTypeIndex];

                    cmd.Add<InsertBarrier>(atlasTexture->GetRenderResource().GetImage(), RS_COPY_DST);

                    for (const auto& elementTexturePair : elementTextures[textureTypeIndex])
                    {
                        const LightmapElement* element = elementTexturePair.first;
                        const Handle<Texture>& elementTexture = elementTexturePair.second;

                        HYP_LOG(Lightmap, Debug, "Blitting element {} (name: {}) to atlas texture {} (dim: {}), at offset {}, dimensions {}",
                            element->index,
                            elementTexture->GetName(),
                            atlasTexture->GetName(),
                            elementTexture->GetRenderResource().GetImage()->GetExtent(),
                            element->offsetCoords,
                            element->dimensions);

                        AssertDebug(element->offsetCoords.x < atlasTexture->GetExtent().x);
                        AssertDebug(element->offsetCoords.y < atlasTexture->GetExtent().y);
                        AssertDebug(element->offsetCoords.x + element->dimensions.x <= atlasTexture->GetExtent().x);
                        AssertDebug(element->offsetCoords.y + element->dimensions.y <= atlasTexture->GetExtent().y);

                        cmd.Add<InsertBarrier>(elementTexture->GetRenderResource().GetImage(), RS_COPY_SRC);

                        cmd.Add<Blit>(
                            elementTexture->GetRenderResource().GetImage(),
                            atlasTexture->GetRenderResource().GetImage(),
                            Rect<uint32> { 0, 0, elementTexture->GetRenderResource().GetImage()->GetExtent().x, elementTexture->GetRenderResource().GetImage()->GetExtent().y },
                            Rect<uint32> { element->offsetCoords.x, element->offsetCoords.y, element->offsetCoords.x + element->dimensions.x, element->offsetCoords.y + element->dimensions.y },
                            0, /* srcMip */
                            0, /* dstMip */
                            0, /* srcFace */
                            0  /* dstFace */
                        );

                        cmd.Add<InsertBarrier>(elementTexture->GetRenderResource().GetImage(), RS_SHADER_RESOURCE);
                    }

                    cmd.Add<InsertBarrier>(atlasTexture->GetRenderResource().GetImage(), RS_SHADER_RESOURCE);
                }
            });

        HYPERION_ASSERT_RESULT(commands.Execute());

        // DEBUGGING: Save each atlas texture to a file for debugging purposes
        HYP_LOG(Lightmap, Info, "Saving atlas textures to disk for debugging");
        for (uint32 textureTypeIndex = 0; textureTypeIndex < uint32(LTT_MAX); textureTypeIndex++)
        {
            if (!atlasTextures[textureTypeIndex])
            {
                continue;
            }

            const Handle<Texture>& atlasTexture = atlasTextures[textureTypeIndex];

            ByteBuffer data;

            if (RendererResult readbackResult = atlasTexture->GetRenderResource().Readback(data); readbackResult.HasError())
            {
                return readbackResult;
            }

            if (data.Empty())
            {
                HYP_LOG(Lightmap, Warning, "Atlas texture {} is empty, skipping save", atlasTexture->GetName());
                continue;
            }

            Bitmap<4> bitmap(atlasTexture->GetExtent().x, atlasTexture->GetExtent().y);
            bitmap.SetPixels(data);

            const String filename = HYP_FORMAT("lightmap_atlas_texture_{}_{}.bmp",
                atlasTexture->GetName(),
                uint32(LightmapTextureType(textureTypeIndex)));

            HYP_LOG(Lightmap, Info, "Writing atlas texture {} to file {}", atlasTexture->GetName(), filename);

            FileByteWriter fileByteWriter { filename };
            bool res = bitmap.Write(&fileByteWriter);

            if (!res)
            {
                HYP_LOG(Lightmap, Error, "Failed to write atlas texture {} to file", atlasTexture->GetName());
            }
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

LightmapVolume::LightmapVolume()
    : LightmapVolume(BoundingBox::Empty())
{
}

LightmapVolume::LightmapVolume(const BoundingBox& aabb)
    : m_aabb(aabb),
      m_atlas(Vec2u(4096, 4096))
{
    m_atlasTextures = {
        { LTT_RADIANCE, Handle<Texture>::empty },
        { LTT_IRRADIANCE, Handle<Texture>::empty }
    };
}

LightmapVolume::~LightmapVolume()
{
    if (IsInitCalled())
    {
        SetReady(false);
    }
}

bool LightmapVolume::AddElement(const LightmapUVMap& uvMap, LightmapElement& outElement, bool shrinkToFit, float downscaleLimit)
{
    Threads::AssertOnThread(g_gameThread);

    const Vec2u elementDimensions = { uvMap.width, uvMap.height };

    return m_atlas.AddElement(elementDimensions, outElement, shrinkToFit, downscaleLimit);
}

const LightmapElement* LightmapVolume::GetElement(uint32 index) const
{
    Threads::AssertOnThread(g_gameThread);

    if (index >= m_atlas.elements.Size())
    {
        return nullptr;
    }

    return &m_atlas.elements[index];
}

bool LightmapVolume::BuildElementTextures(const LightmapUVMap& uvMap, uint32 index)
{
    Threads::AssertOnThread(g_gameThread);

    if (index >= m_atlas.elements.Size())
    {
        return false;
    }

    LightmapElement& element = m_atlas.elements[index];

    const Vec2u elementDimensions = element.dimensions;

    FixedArray<Bitmap<4, float>, uint32(LTT_MAX)> bitmaps = {
        uvMap.ToBitmapRadiance(),  /* RADIANCE */
        uvMap.ToBitmapIrradiance() /* IRRADIANCE */
    };

    element.entries.Resize(uint32(LTT_MAX));

    static const char* textureTypeNames[uint32(LTT_MAX)] = { "R", "I" };

    for (uint32 i = 0; i < uint32(LTT_MAX); i++)
    {
        element.entries[i].type = LightmapTextureType(i);

        Handle<Texture> texture = CreateObject<Texture>(TextureData {
            TextureDesc {
                TT_TEX2D,
                TF_RGBA32F,
                Vec3u { elementDimensions, 1 },
                TFM_LINEAR,
                TFM_LINEAR,
                TWM_REPEAT },
            ByteBuffer(bitmaps[i].GetUnpackedFloats().ToByteView()) });

        texture->SetName(NAME_FMT("LightmapVolumeTexture_{}_{}_{}", m_uuid, element.index, textureTypeNames[i]));

        element.entries[i].texture = std::move(texture);
    }

    if (IsInitCalled())
    {
        // Initalize textures for the entries in the element
        for (LightmapElementTextureEntry& entry : element.entries)
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

void LightmapVolume::Init()
{
    for (LightmapElement& element : m_atlas.elements)
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

void LightmapVolume::UpdateRenderProxy(IRenderProxy* proxy)
{
    RenderProxyLightmapVolume* proxyCasted = static_cast<RenderProxyLightmapVolume*>(proxy);

    proxyCasted->lightmapVolume = WeakHandleFromThis();

    proxyCasted->bufferData.aabbMax = Vec4f(m_aabb.max, 1.0f);
    proxyCasted->bufferData.aabbMin = Vec4f(m_aabb.min, 1.0f);
    proxyCasted->bufferData.textureIndex = ~0u; // @TODO: Set the correct texture index based on the element
}

void LightmapVolume::UpdateAtlasTextures()
{
    HYP_LOG(Lightmap, Debug, "Updating atlas textures for LightmapVolume {}", m_uuid);

    static const char* textureTypeNames[uint32(LTT_MAX)] = { "R", "I" };

    Array<Handle<Texture>> elementTextures;
    elementTextures.Resize(uint32(LTT_MAX));

    for (uint32 i = 0; i < uint32(LTT_MAX); i++)
    {
        LightmapTextureType textureType = LightmapTextureType(i);

        Handle<Texture>& texture = m_atlasTextures[textureType];

        if (!texture.IsValid())
        {
            texture = CreateObject<Texture>(TextureDesc {
                TT_TEX2D,
                TF_RGBA8,
                Vec3u { m_atlas.atlasDimensions, 1 },
                TFM_LINEAR,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE });

            texture->SetName(NAME_FMT("LightmapVolumeAtlasTexture_{}_{}_{}", m_uuid, uint32(textureType), textureTypeNames[i]));
        }

        InitObject(texture);

        elementTextures[i] = texture;
    }

    if (IsInitCalled())
    {
        PUSH_RENDER_COMMAND(
            BakeLightmapVolumeTexture,
            WeakHandleFromThis(),
            m_atlas.elements,
            elementTextures);
    }
}

} // namespace hyperion
