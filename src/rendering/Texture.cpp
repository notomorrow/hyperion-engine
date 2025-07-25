/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Texture.hpp>

#include <rendering/RenderTexture.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/RenderImage.hpp>
#include <rendering/RenderSampler.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/TextureAsset.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <EngineGlobals.hpp>
#include <Engine.hpp>

namespace hyperion {

class Texture;
class TextureMipmapRenderer;

const FixedArray<Pair<Vec3f, Vec3f>, 6> Texture::cubemapDirections = {
    Pair<Vec3f, Vec3f> { Vec3f { 1, 0, 0 }, Vec3f { 0, 1, 0 } },
    Pair<Vec3f, Vec3f> { Vec3f { -1, 0, 0 }, Vec3f { 0, 1, 0 } },
    Pair<Vec3f, Vec3f> { Vec3f { 0, 1, 0 }, Vec3f { 0, 0, -1 } },
    Pair<Vec3f, Vec3f> { Vec3f { 0, -1, 0 }, Vec3f { 0, 0, 1 } },
    Pair<Vec3f, Vec3f> { Vec3f { 0, 0, 1 }, Vec3f { 0, 1, 0 } },
    Pair<Vec3f, Vec3f> { Vec3f { 0, 0, -1 }, Vec3f { 0, 1, 0 } }
};

static const Name g_nameTextureDefault = NAME("<unnamed texture>");

#pragma region Texture

Texture::Texture()
    : Texture(TextureDesc {
          TT_TEX2D,
          TF_RGBA8,
          Vec3u { 1, 1, 1 },
          TFM_NEAREST,
          TFM_NEAREST,
          TWM_CLAMP_TO_EDGE })
{
}

Texture::Texture(const TextureDesc& textureDesc)
    : m_renderResource(nullptr),
      m_asset(CreateObject<TextureAsset>(g_nameTextureDefault, TextureData { textureDesc }))
{
    SetName(g_nameTextureDefault);
}

Texture::Texture(const TextureData& textureData)
    : m_renderResource(nullptr),
      m_asset(CreateObject<TextureAsset>(g_nameTextureDefault, textureData))
{
    SetName(g_nameTextureDefault);
}

Texture::Texture(const Handle<TextureAsset>& asset)
    : m_renderResource(nullptr),
      m_asset(asset)
{
    SetName(g_nameTextureDefault);
}

Texture::~Texture()
{
    m_renderPersistent.Reset();

    // temp shit
    if (m_renderResource)
    {
        m_renderResource->DecRef();
        FreeResource(m_renderResource);
        m_renderResource = nullptr;
    }
}

void Texture::Init()
{
    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]()
        {
            m_renderPersistent.Reset();

            if (m_renderResource)
            {
                // temp shit
                m_renderResource->DecRef();
                FreeResource(m_renderResource);
                m_renderResource = nullptr;
            }
        }));

    if (m_asset.IsValid() && !m_asset->IsRegistered())
    {
        if (!m_asset->GetName().IsValid())
        {
            m_asset->Rename(m_name);
        }

        g_assetManager->GetAssetRegistry()->RegisterAsset("$Memory/Media/Textures", m_asset);
    }

    m_renderResource = AllocateResource<RenderTexture>(this);

    // temp shit
    m_renderResource->IncRef();

    SetReady(true);
}

void Texture::SetName(Name name)
{
    if (name == m_name)
    {
        return;
    }

    m_name = name;

    if (m_asset.IsValid() && !m_asset->IsRegistered() && IsInitCalled())
    {
        if (!m_asset->GetName().IsValid())
        {
            m_asset->Rename(m_name);
        }

        g_assetManager->GetAssetRegistry()->RegisterAsset("$Memory/Media/Textures", m_asset);
    }
}

const TextureDesc& Texture::GetTextureDesc() const
{
    static const TextureDesc defaultTextureDesc {};
    return m_asset.IsValid() ? m_asset->GetTextureDesc() : defaultTextureDesc;
}

void Texture::SetTextureDesc(const TextureDesc& textureDesc)
{
    Mutex::Guard guard(m_readbackMutex);

    TextureDesc currentTextureDesc = GetTextureDesc();

    if (currentTextureDesc == textureDesc)
    {
        return;
    }

    // create new asset
    if (m_asset.IsValid())
    {
        Handle<TextureAsset> prevAsset = m_asset;

        Handle<AssetPackage> package = prevAsset->GetPackage();

        ResourceHandle resourceHandle(*prevAsset->GetResource());

        TextureData textureData = std::move(*prevAsset->GetTextureData());
        textureData.desc = textureDesc;

        m_asset = CreateObject<TextureAsset>(prevAsset->GetName(), textureData);

        if (package.IsValid())
        {
            package->RemoveAssetObject(prevAsset);
            package->AddAssetObject(m_asset);
        }
    }
    else
    {
        m_asset = CreateObject<TextureAsset>(GetName(), TextureData { textureDesc });
    }

    if (IsInitCalled() && !m_asset->IsRegistered())
    {
        g_assetManager->GetAssetRegistry()->RegisterAsset("$Memory/Media/Textures", m_asset);
    }
}

void Texture::GenerateMipmaps()
{
    AssertReady();

    m_renderResource->IncRef();
    m_renderResource->RenderMipmaps();
    m_renderResource->DecRef();
}

void Texture::Readback()
{
    Mutex::Guard guard(m_readbackMutex);

    Readback_Internal();
}

void Texture::Readback_Internal()
{
    AssertReady();

    // ByteBuffer resultByteBuffer;
    // m_renderResource->Readback(resultByteBuffer);

    // const SizeType expected = m_textureDesc.GetByteSize();
    // const SizeType real = resultByteBuffer.Size();

    // Assert(expected == real, "Failed to readback texture: expected size: %llu, got %llu", expected, real);

    // m_streamedTextureDataResourceHandle.Reset();

    // m_streamedTextureData = MakeRefCountedPtr<StreamedTextureData>(TextureData {
    //                                                                    m_textureDesc,
    //                                                                    std::move(resultByteBuffer) },
    //     m_streamedTextureDataResourceHandle);

    HYP_NOT_IMPLEMENTED();
}

void Texture::Resize(const Vec3u& extent)
{
    TextureDesc textureDesc = GetTextureDesc();

    if (textureDesc.extent == extent)
    {
        return;
    }

    textureDesc.extent = extent;

    SetTextureDesc(textureDesc);

    if (m_renderResource)
    {
        m_renderResource->Resize(extent);
    }
}

Vec4f Texture::Sample(Vec3f uvw, uint32 faceIndex)
{
    if (!IsReady())
    {
        HYP_LOG(Texture, Warning, "Texture is not ready, cannot sample");

        return Vec4f::Zero();
    }

    if (faceIndex >= NumFaces())
    {
        HYP_LOG(Texture, Warning, "Face index out of bounds: {} >= {}", faceIndex, NumFaces());

        return Vec4f::Zero();
    }

    if (!m_asset.IsValid())
    {
        HYP_LOG(Texture, Warning, "Texture asset is not valid, cannot sample");

        return Vec4f::Zero();
    }

    ResourceHandle resourceHandle;
    TextureData* textureData = nullptr;

    {
        Mutex::Guard guard(m_readbackMutex);

        if (!m_asset->IsLoaded())
        {
            HYP_LOG(Texture, Warning, "Texture does not have data loaded, attempting readback...");

            Readback_Internal();

            if (!m_asset->IsLoaded())
            {
                HYP_LOG(Texture, Warning, "Texture readback failed. Sample will return zero.");

                return Vec4f::Zero();
            }
        }

        resourceHandle = ResourceHandle(*m_asset->GetResource());
        textureData = m_asset->GetTextureData();

        if (!textureData || textureData->imageData.Size() == 0)
        {
            HYP_LOG(Texture, Warning, "Texture buffer is empty; forcing readback.");

            resourceHandle.Reset();

            Readback_Internal();

            if (!m_asset->IsLoaded())
            {
                HYP_LOG(Texture, Warning, "Texture readback failed. Sample will return zero.");

                return Vec4f::Zero();
            }

            resourceHandle = ResourceHandle(*m_asset->GetResource());

            textureData = m_asset->GetTextureData();

            if (!textureData || textureData->imageData.Size() == 0)
            {
                HYP_LOG(Texture, Warning, "Texture buffer is still empty after readback; sample will return zero.");

                return Vec4f::Zero();
            }
        }
    }

    const Vec3u coord = {
        uint32(uvw.x * float(textureData->desc.extent.x - 1) + 0.5f),
        uint32(uvw.y * float(textureData->desc.extent.y - 1) + 0.5f),
        uint32(uvw.z * float(textureData->desc.extent.z - 1) + 0.5f)
    };

    const uint32 bytesPerComponent = BytesPerComponent(textureData->desc.format);

    if (bytesPerComponent != 1)
    {
        HYP_LOG(Texture, Warning, "Unsupported bytes per component to use with Sample(): {}", bytesPerComponent);

        HYP_BREAKPOINT;

        return Vec4f::Zero();
    }

    const uint32 numComponents = NumComponents(textureData->desc.format);

    const uint32 index = faceIndex * (textureData->desc.extent.x * textureData->desc.extent.y * textureData->desc.extent.z * bytesPerComponent * numComponents)
        + coord.z * (textureData->desc.extent.x * textureData->desc.extent.y * bytesPerComponent * numComponents)
        + coord.y * (textureData->desc.extent.x * bytesPerComponent * numComponents)
        + coord.x * bytesPerComponent * numComponents;

    if (index >= textureData->imageData.Size())
    {
        HYP_LOG(Texture, Warning, "Index out of bounds, index: {}, buffer size: {}, coord: {}, dimensions: {}, num faces: {}", index, textureData->imageData.Size(),
            coord, textureData->desc.extent, NumFaces());

        return Vec4f::Zero();
    }

    const ubyte* data = textureData->imageData.Data() + index;

    switch (numComponents)
    {
    case 1:
        return Vec4f(float(data[0]) / 255.0f);
    case 2:
        return Vec4f(float(data[0]) / 255.0f, float(data[1]) / 255.0f, 0.0f, 1.0f);
    case 3:
        return Vec4f(float(data[0]) / 255.0f, float(data[1]) / 255.0f, float(data[2]) / 255.0f, 1.0f);
    case 4:
        return Vec4f(float(data[0]) / 255.0f, float(data[1]) / 255.0f, float(data[2]) / 255.0f, float(data[3]) / 255.0f);
    default: // should never happen
        HYP_LOG(Texture, Error, "Unsupported number of components: {}", numComponents);

        return Vec4f::Zero();
    }
}

Vec4f Texture::Sample2D(Vec2f uv)
{
    if (GetType() != TT_TEX2D)
    {
        HYP_LOG(Texture, Warning, "Unsupported texture type to use with Sample2D(): {}", GetType());

        return Vec4f::Zero();
    }

    return Sample(Vec3f { uv.x, uv.y, 0.0f }, 0);
}

/// https://www.gamedev.net/forums/topic/687535-implementing-a-cube-map-lookup-function/5337472/
Vec4f Texture::SampleCube(Vec3f direction)
{
    if (GetType() != TT_CUBEMAP)
    {
        HYP_LOG(Texture, Warning, "Unsupported texture type to use with SampleCube(): {}", GetType());

        return Vec4f::Zero();
    }

    Vec3f absDir = MathUtil::Abs(direction);
    uint32 faceIndex = 0;

    float mag;
    Vec2f uv;

    if (absDir.z >= absDir.x && absDir.z >= absDir.y)
    {
        mag = absDir.z;

        if (direction.z < 0.0f)
        {
            faceIndex = 5;
            uv = Vec2f(-direction.x, -direction.y);
        }
        else
        {
            faceIndex = 4;
            uv = Vec2f(direction.x, -direction.y);
        }
    }
    else if (absDir.y >= absDir.x)
    {
        mag = absDir.y;

        if (direction.y < 0.0f)
        {
            faceIndex = 3;
            uv = Vec2f(direction.x, -direction.z);
        }
        else
        {
            faceIndex = 2;
            uv = Vec2f(direction.x, direction.z);
        }
    }
    else
    {
        mag = absDir.x;

        if (direction.x < 0.0f)
        {
            faceIndex = 1;
            uv = Vec2f(direction.z, -direction.y);
        }
        else
        {
            faceIndex = 0;
            uv = Vec2f(-direction.z, -direction.y);
        }
    }

    return Sample(Vec3f { uv / mag * 0.5f + 0.5f, 0.0f }, faceIndex);
}

void Texture::SetPersistentRenderResourceEnabled(bool enabled)
{
    AssertReady();

    if (enabled)
    {
        if (!m_renderPersistent)
        {
            m_renderPersistent = ResourceHandle(*m_renderResource);
        }
    }
    else
    {
        m_renderPersistent.Reset();
    }
}

#pragma endregion Texture

} // namespace hyperion
