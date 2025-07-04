/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/SafeDeleter.hpp>

#include <rendering/backend/RenderBackend.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RenderConfig.hpp>

#include <scene/Material.hpp>
#include <scene/Texture.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/utilities/ByteUtil.hpp>
#include <core/profiling/ProfileScope.hpp>

#include <EngineGlobals.hpp>
#include <Engine.hpp>
#include <Types.hpp>

namespace hyperion {

#pragma region RenderMaterial

RenderMaterial::RenderMaterial(Material* material)
    : m_material(material),
      m_bufferData {}
{
}

RenderMaterial::RenderMaterial(RenderMaterial&& other) noexcept
    : RenderResourceBase(static_cast<RenderResourceBase&&>(other)),
      m_material(other.m_material),
      m_textures(std::move(other.m_textures)),
      m_boundTextureIds(std::move(other.m_boundTextureIds)),
      m_bufferData(std::move(other.m_bufferData))
{
    other.m_material = nullptr;
}

RenderMaterial::~RenderMaterial() = default;

void RenderMaterial::Initialize_Internal()
{
    HYP_SCOPE;

    Assert(m_material != nullptr);

    m_renderTextures.Reserve(m_textures.Size());

    for (const Pair<MaterialTextureKey, Handle<Texture>>& it : m_textures)
    {
        if (const Handle<Texture>& texture = it.second)
        {
            Assert(texture->IsReady());

            m_renderTextures.Set(texture->Id(), TResourceHandle<RenderTexture>(texture->GetRenderResource()));
        }
    }

    UpdateBufferData();

    HYP_LOG(Material, Debug, "Initializing RenderMaterial: {}", (void*)this);

    if (!g_renderBackend->GetRenderConfig().IsBindlessSupported())
    {
        CreateDescriptorSets();
    }
}

void RenderMaterial::Destroy_Internal()
{
    HYP_SCOPE;

    Assert(m_material != nullptr);

    m_renderTextures.Clear();

    HYP_LOG(Material, Debug, "Destroying RenderMaterial: {}", (void*)this);

    if (!g_renderBackend->GetRenderConfig().IsBindlessSupported())
    {
        DestroyDescriptorSets();
    }
}

void RenderMaterial::Update_Internal()
{
    HYP_SCOPE;

    Assert(m_material != nullptr);

    // static const bool useBindlessTextures = g_renderBackend->GetRenderConfig().IsBindlessSupported();

    // if (!useBindlessTextures)
    // {

    //     auto setMaterialTexture = [this](const Handle<Material>& material, uint32 textureIndex, const Handle<Texture>& texture)
    //     {
    //         for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
    //         {
    //             const DescriptorSetRef& descriptorSet = g_engine->ForBoundMaterialManager()->GetDescriptorSet(this, frameIndex);
    //             Assert(descriptorSet != nullptr);

    //             if (texture.IsValid())
    //             {
    //                 Assert(texture->GetRenderResource().GetImageView() != nullptr);

    //                 descriptorSet->SetElement(NAME("Textures"), textureIndex, texture->GetRenderResource().GetImageView());
    //             }
    //             else
    //             {
    //                 descriptorSet->SetElement(NAME("Textures"), textureIndex, g_renderGlobalState->placeholderData->GetImageView2D1x1R8());
    //             }
    //         }
    //     };

    //     Handle<Material> materialLocked = m_material->HandleFromThis();

    //     HYP_LOG(Material, Debug, "Updating Material {} (name: {})", materialLocked->Id(), *materialLocked->GetName());

    //     for (const auto& it : m_textures)
    //     {
    //         const uint32 textureIndex = uint32(Material::TextureSet::EnumToOrdinal(it.first));

    //         if (textureIndex >= maxBoundTextures)
    //         {
    //             HYP_LOG(Material, Warning, "Texture index {} is out of bounds of max bound textures ({})", textureIndex, maxBoundTextures);

    //             continue;
    //         }

    //         const Handle<Texture>& texture = it.second;

    //         setMaterialTexture(materialLocked, textureIndex, texture);
    //     }
    // }
}

GpuBufferHolderBase* RenderMaterial::GetGpuBufferHolder() const
{
    return g_renderGlobalState->gpuBuffers[GRB_MATERIALS];
}

void RenderMaterial::CreateDescriptorSets()
{
    HYP_SCOPE;

    Assert(m_material != nullptr);

    // FixedArray<Handle<Texture>, maxBoundTextures> textureBindings;

    // for (const Pair<MaterialTextureKey, Handle<Texture>>& it : m_textures)
    // {
    //     const SizeType textureIndex = Material::TextureSet::EnumToOrdinal(it.first);

    //     if (textureIndex >= textureBindings.Size())
    //     {
    //         continue;
    //     }

    //     if (const Handle<Texture>& texture = it.second)
    //     {
    //         textureBindings[textureIndex] = texture;
    //     }
    // }

    // m_descriptorSets = g_engine->ForBoundMaterialManager()->AddMaterial(this, std::move(textureBindings));
}

void RenderMaterial::DestroyDescriptorSets()
{
    HYP_SCOPE;

    Assert(m_material != nullptr);
}

void RenderMaterial::UpdateBufferData()
{
    HYP_SCOPE;

    Assert(m_bufferIndex != ~0u);

    // static const bool useBindlessTextures = g_renderBackend->GetRenderConfig().IsBindlessSupported();

    // m_bufferData.textureUsage = 0;
    // Memory::MemSet(m_bufferData.textureIndex, 0, sizeof(m_bufferData.textureIndex));

    // if (m_boundTextureIds.Any())
    // {
    //     for (SizeType i = 0; i < m_boundTextureIds.Size(); i++)
    //     {
    //         if (m_boundTextureIds[i] != ObjId<Texture>::invalid)
    //         {
    //             if (useBindlessTextures)
    //             {
    //                 m_bufferData.textureIndex[i] = m_boundTextureIds[i].ToIndex();
    //             }
    //             else
    //             {
    //                 m_bufferData.textureIndex[i] = i;
    //             }

    //             m_bufferData.textureUsage |= 1 << i;
    //         }
    //     }
    // }

    // *static_cast<MaterialShaderData*>(m_bufferAddress) = m_bufferData;

    // GetGpuBufferHolder()->MarkDirty(m_bufferIndex);
}

void RenderMaterial::SetTexture(MaterialTextureKey textureKey, const Handle<Texture>& texture)
{
    HYP_SCOPE;

    Execute([this, textureKey, texture]()
        {
            auto it = m_textures.FindAs(textureKey);

            if (it != m_textures.End())
            {
                if (it->second == texture)
                {
                    return;
                }

                m_renderTextures.Erase(it->second->Id());
            }

            m_textures.Set(textureKey, texture);

            if (IsInitialized())
            {
                if (texture.IsValid())
                {
                    Assert(texture->IsReady());

                    m_renderTextures.Set(texture->Id(), TResourceHandle<RenderTexture>(texture->GetRenderResource()));
                }

                UpdateBufferData();
            }
        });
}

void RenderMaterial::SetTextures(FlatMap<MaterialTextureKey, Handle<Texture>>&& textures)
{
    HYP_SCOPE;

    Execute([this, textures = std::move(textures)]()
        {
            m_renderTextures.Clear();

            m_textures = std::move(textures);

            if (IsInitialized())
            {
                for (const Pair<MaterialTextureKey, Handle<Texture>>& it : m_textures)
                {
                    if (it.second.IsValid())
                    {
                        Assert(it.second->IsReady());

                        m_renderTextures.Set(it.second->Id(), TResourceHandle<RenderTexture>(it.second->GetRenderResource()));
                    }
                }

                UpdateBufferData();
            }
        });
}

void RenderMaterial::SetBoundTextureIDs(const Array<ObjId<Texture>>& boundTextureIds)
{
    HYP_SCOPE;

    Execute([this, boundTextureIds]()
        {
            m_boundTextureIds = boundTextureIds;

            if (IsInitialized())
            {
                UpdateBufferData();
            }
        });
}

void RenderMaterial::SetBufferData(const MaterialShaderData& bufferData)
{
    HYP_SCOPE;

    Execute([this, bufferData]()
        {
            m_bufferData = bufferData;

            if (IsInitialized())
            {
                UpdateBufferData();
            }
        });
}

#pragma endregion RenderMaterial

#pragma region MaterialDescriptorSetManager

MaterialDescriptorSetManager::MaterialDescriptorSetManager()
{
}

MaterialDescriptorSetManager::~MaterialDescriptorSetManager()
{
    SafeRelease(std::move(m_fallbackMaterialDescriptorSets));

    for (auto& it : m_materialDescriptorSets)
    {
        SafeRelease(std::move(it.second));
    }

    m_materialDescriptorSets.Clear();
}

void MaterialDescriptorSetManager::CreateFallbackMaterialDescriptorSet()
{
    if (g_renderBackend->GetRenderConfig().IsBindlessSupported())
    {
        return;
    }

    const DescriptorSetDeclaration* decl = g_renderGlobalState->GlobalDescriptorTable->GetDeclaration()->FindDescriptorSetDeclaration(NAME("Material"));
    Assert(decl != nullptr);

    const DescriptorSetLayout layout { decl };

    for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
    {
        m_fallbackMaterialDescriptorSets[frameIndex] = g_renderBackend->MakeDescriptorSet(layout);
        m_fallbackMaterialDescriptorSets[frameIndex]->SetDebugName(NAME_FMT("MaterialDescriptorSet_INVALID_{}", frameIndex));

        for (uint32 textureIndex = 0; textureIndex < maxBoundTextures; textureIndex++)
        {
            m_fallbackMaterialDescriptorSets[frameIndex]->SetElement(NAME("Textures"), textureIndex, g_renderGlobalState->placeholderData->DefaultTexture2D->GetRenderResource().GetImageView());
        }

        DeferCreate(m_fallbackMaterialDescriptorSets[frameIndex]);
    }

    m_materialDescriptorSets.Set(~0u, m_fallbackMaterialDescriptorSets);
}

const DescriptorSetRef& MaterialDescriptorSetManager::ForBoundMaterial(const Material* material, uint32 frameIndex)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);

    uint32 boundIndex = ~0u;

    if (material)
    {
        boundIndex = RenderApi_RetrieveResourceBinding(material);

        AssertDebug(boundIndex != ~0u, "Material with ID: %u is not bound for this frame! (Current frame: %u)", material->Id().Value(), RenderApi_GetFrameIndex_RenderThread());
    }

    if (boundIndex != ~0u)
    {
        const auto it = m_materialDescriptorSets.Find(boundIndex);

        if (it != m_materialDescriptorSets.End() && it->second[frameIndex].IsValid())
        {

            return it->second[frameIndex];
        }
    }

    AssertDebug(m_fallbackMaterialDescriptorSets[frameIndex].IsValid()
        && m_fallbackMaterialDescriptorSets[frameIndex]->IsCreated());

    return m_fallbackMaterialDescriptorSets[frameIndex];
}

FixedArray<DescriptorSetRef, maxFramesInFlight> MaterialDescriptorSetManager::Allocate(uint32 boundIndex)
{
    if (boundIndex == ~0u)
    {
        return {};
    }

    Threads::AssertOnThread(g_renderThread);

    const DescriptorSetDeclaration* decl = g_renderGlobalState->GlobalDescriptorTable->GetDeclaration()->FindDescriptorSetDeclaration(NAME("Material"));
    Assert(decl != nullptr);

    DescriptorSetLayout layout { decl };

    FixedArray<DescriptorSetRef, maxFramesInFlight> descriptorSets;

    for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
    {
        DescriptorSetRef descriptorSet = g_renderBackend->MakeDescriptorSet(layout);

#ifdef HYP_DEBUG_MODE
        descriptorSet->SetDebugName(NAME_FMT("MaterialDescriptorSet_{}_{}", boundIndex, frameIndex));
#endif

        for (uint32 textureIndex = 0; textureIndex < maxBoundTextures; textureIndex++)
        {
            descriptorSet->SetElement(NAME("Textures"), textureIndex, g_renderGlobalState->placeholderData->DefaultTexture2D->GetRenderResource().GetImageView());
        }

        descriptorSets[frameIndex] = std::move(descriptorSet);
    }

    for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
    {
        HYPERION_ASSERT_RESULT(descriptorSets[frameIndex]->Create());
    }

    auto it = m_materialDescriptorSets.Find(boundIndex);
    if (it != m_materialDescriptorSets.End())
    {
        SafeRelease(std::move(it->second));
    }

    m_materialDescriptorSets[boundIndex] = descriptorSets;

    return descriptorSets;
}

FixedArray<DescriptorSetRef, maxFramesInFlight> MaterialDescriptorSetManager::Allocate(
    uint32 boundIndex,
    Span<const uint32> textureIndirectIndices,
    Span<const Handle<Texture>> textures)
{
    if (boundIndex == ~0u)
    {
        return {};
    }

    Threads::AssertOnThread(g_renderThread);

    const DescriptorSetDeclaration* decl = g_renderGlobalState->GlobalDescriptorTable->GetDeclaration()->FindDescriptorSetDeclaration(NAME("Material"));
    Assert(decl != nullptr);

    const DescriptorSetLayout layout { decl };

    FixedArray<DescriptorSetRef, maxFramesInFlight> descriptorSets;

    for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
    {
        DescriptorSetRef descriptorSet = g_renderBackend->MakeDescriptorSet(layout);

#ifdef HYP_DEBUG_MODE
        descriptorSet->SetDebugName(NAME_FMT("MaterialDescriptorSet_{}_{}", boundIndex, frameIndex));
#endif

        // set initial placeholder elements that will get overridden
        for (uint32 i = 0; i < maxBoundTextures; i++)
        {
            descriptorSet->SetElement(NAME("Textures"), i, g_renderGlobalState->placeholderData->DefaultTexture2D->GetRenderResource().GetImageView());
        }

        for (uint32 slot = 0; slot < uint32(textureIndirectIndices.Size()); slot++)
        {
            const uint32 textureIndex = textureIndirectIndices[slot];

            if (textureIndex == ~0u)
            {
                continue;
            }

            AssertDebug(textureIndex < textures.Size(),
                "Texture index %u is out of bounds of textures array size %llu",
                textureIndex, textures.Size());

            const Handle<Texture>& texture = textures[textureIndex];

            if (texture.IsValid() && texture->GetRenderResource().GetImageView() != nullptr)
            {
                descriptorSet->SetElement(NAME("Textures"), textureIndex, texture->GetRenderResource().GetImageView());
            }
        }

        descriptorSets[frameIndex] = std::move(descriptorSet);
    }

    for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
    {
        HYPERION_ASSERT_RESULT(descriptorSets[frameIndex]->Create());
    }

    auto it = m_materialDescriptorSets.Find(boundIndex);
    if (it != m_materialDescriptorSets.End())
    {
        SafeRelease(std::move(it->second));
    }

    m_materialDescriptorSets[boundIndex] = descriptorSets;

    return descriptorSets;
}

void MaterialDescriptorSetManager::Remove(uint32 boundIndex)
{
    Threads::AssertOnThread(g_renderThread);

    if (boundIndex == ~0u)
    {
        return;
    }

    const auto it = m_materialDescriptorSets.Find(boundIndex);

    if (it != m_materialDescriptorSets.End())
    {
        for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
        {
            SafeRelease(std::move(it->second[frameIndex]));
        }

        m_materialDescriptorSets.Erase(it);
    }
}

#pragma endregion MaterialDescriptorSetManager

HYP_DESCRIPTOR_SSBO_COND(Object, MaterialsBuffer, 1, ~0u, false, !g_renderBackend->GetRenderConfig().ShouldCollectUniqueDrawCallPerMaterial());
HYP_DESCRIPTOR_SSBO_COND(Object, MaterialsBuffer, 1, sizeof(MaterialShaderData), true, g_renderBackend->GetRenderConfig().ShouldCollectUniqueDrawCallPerMaterial());

} // namespace hyperion
