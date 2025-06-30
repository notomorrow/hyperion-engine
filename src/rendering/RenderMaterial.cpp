/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/SafeDeleter.hpp>

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

    AssertThrow(m_material != nullptr);

    m_renderTextures.Reserve(m_textures.Size());

    for (const Pair<MaterialTextureKey, Handle<Texture>>& it : m_textures)
    {
        if (const Handle<Texture>& texture = it.second)
        {
            AssertThrow(texture->IsReady());

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

    AssertThrow(m_material != nullptr);

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

    AssertThrow(m_material != nullptr);

    static const bool useBindlessTextures = g_renderBackend->GetRenderConfig().IsBindlessSupported();

    if (!useBindlessTextures)
    {

        auto setMaterialTexture = [this](const Handle<Material>& material, uint32 textureIndex, const Handle<Texture>& texture)
        {
            for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
            {
                const DescriptorSetRef& descriptorSet = g_engine->GetMaterialDescriptorSetManager()->GetDescriptorSet(this, frameIndex);
                AssertThrow(descriptorSet != nullptr);

                if (texture.IsValid())
                {
                    AssertThrow(texture->GetRenderResource().GetImageView() != nullptr);

                    descriptorSet->SetElement(NAME("Textures"), textureIndex, texture->GetRenderResource().GetImageView());
                }
                else
                {
                    descriptorSet->SetElement(NAME("Textures"), textureIndex, g_renderGlobalState->PlaceholderData->GetImageView2D1x1R8());
                }
            }
        };

        Handle<Material> materialLocked = m_material->HandleFromThis();

        HYP_LOG(Material, Debug, "Updating Material {} (name: {})", materialLocked->Id(), *materialLocked->GetName());

        for (const auto& it : m_textures)
        {
            const uint32 textureIndex = uint32(Material::TextureSet::EnumToOrdinal(it.first));

            if (textureIndex >= maxBoundTextures)
            {
                HYP_LOG(Material, Warning, "Texture index {} is out of bounds of max bound textures ({})", textureIndex, maxBoundTextures);

                continue;
            }

            const Handle<Texture>& texture = it.second;

            setMaterialTexture(materialLocked, textureIndex, texture);
        }
    }
}

GpuBufferHolderBase* RenderMaterial::GetGpuBufferHolder() const
{
    return g_renderGlobalState->gpuBuffers[GRB_MATERIALS];
}

void RenderMaterial::CreateDescriptorSets()
{
    HYP_SCOPE;

    AssertThrow(m_material != nullptr);

    FixedArray<Handle<Texture>, maxBoundTextures> textureBindings;

    for (const Pair<MaterialTextureKey, Handle<Texture>>& it : m_textures)
    {
        const SizeType textureIndex = Material::TextureSet::EnumToOrdinal(it.first);

        if (textureIndex >= textureBindings.Size())
        {
            continue;
        }

        if (const Handle<Texture>& texture = it.second)
        {
            textureBindings[textureIndex] = texture;
        }
    }

    m_descriptorSets = g_engine->GetMaterialDescriptorSetManager()->AddMaterial(this, std::move(textureBindings));
}

void RenderMaterial::DestroyDescriptorSets()
{
    HYP_SCOPE;

    AssertThrow(m_material != nullptr);

    g_engine->GetMaterialDescriptorSetManager()->RemoveMaterial(this);
    m_descriptorSets = {};
}

void RenderMaterial::UpdateBufferData()
{
    HYP_SCOPE;

    AssertThrow(m_bufferIndex != ~0u);

    static const bool useBindlessTextures = g_renderBackend->GetRenderConfig().IsBindlessSupported();

    m_bufferData.textureUsage = 0;
    Memory::MemSet(m_bufferData.textureIndex, 0, sizeof(m_bufferData.textureIndex));

    if (m_boundTextureIds.Any())
    {
        for (SizeType i = 0; i < m_boundTextureIds.Size(); i++)
        {
            if (m_boundTextureIds[i] != ObjId<Texture>::invalid)
            {
                if (useBindlessTextures)
                {
                    m_bufferData.textureIndex[i] = m_boundTextureIds[i].ToIndex();
                }
                else
                {
                    m_bufferData.textureIndex[i] = i;
                }

                m_bufferData.textureUsage |= 1 << i;
            }
        }
    }

    *static_cast<MaterialShaderData*>(m_bufferAddress) = m_bufferData;

    GetGpuBufferHolder()->MarkDirty(m_bufferIndex);
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
                    AssertThrow(texture->IsReady());

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
                        AssertThrow(it.second->IsReady());

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
    : m_pendingAdditionFlag { false },
      m_descriptorSetsToUpdateFlag { 0 }
{
}

MaterialDescriptorSetManager::~MaterialDescriptorSetManager()
{
    for (auto& it : m_materialDescriptorSets)
    {
        SafeRelease(std::move(it.second));
    }

    m_materialDescriptorSets.Clear();

    m_pendingMutex.Lock();

    for (auto& it : m_pendingAddition)
    {
        SafeRelease(std::move(it.second));
    }

    m_pendingAddition.Clear();
    m_pendingRemoval.Clear();

    m_pendingMutex.Unlock();
}

void MaterialDescriptorSetManager::CreateInvalidMaterialDescriptorSet()
{
    if (g_renderBackend->GetRenderConfig().IsBindlessSupported())
    {
        return;
    }

    const DescriptorSetDeclaration* decl = g_renderGlobalState->GlobalDescriptorTable->GetDeclaration()->FindDescriptorSetDeclaration(NAME("Material"));
    AssertThrow(decl != nullptr);

    const DescriptorSetLayout layout { decl };

    for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
    {
        m_invalidMaterialDescriptorSets[frameIndex] = g_renderBackend->MakeDescriptorSet(layout);
        m_invalidMaterialDescriptorSets[frameIndex]->SetDebugName(NAME_FMT("MaterialDescriptorSet_INVALID_{}", frameIndex));

        for (uint32 textureIndex = 0; textureIndex < maxBoundTextures; textureIndex++)
        {
            m_invalidMaterialDescriptorSets[frameIndex]->SetElement(NAME("Textures"), textureIndex, g_renderGlobalState->PlaceholderData->GetImageView2D1x1R8());
        }

        DeferCreate(m_invalidMaterialDescriptorSets[frameIndex]);
    }

    m_materialDescriptorSets.Set(nullptr, m_invalidMaterialDescriptorSets);
}

const DescriptorSetRef& MaterialDescriptorSetManager::GetDescriptorSet(const RenderMaterial* material, uint32 frameIndex) const
{
    if (!material)
    {
        return m_invalidMaterialDescriptorSets[frameIndex];
    }

    Threads::AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);

    const auto it = m_materialDescriptorSets.FindAs(material);

    if (it == m_materialDescriptorSets.End())
    {
        return DescriptorSetRef::unset;
    }

    return it->second[frameIndex];
}

FixedArray<DescriptorSetRef, maxFramesInFlight> MaterialDescriptorSetManager::AddMaterial(RenderMaterial* material)
{
    if (!material)
    {
        return {};
    }

    const DescriptorSetDeclaration* decl = g_renderGlobalState->GlobalDescriptorTable->GetDeclaration()->FindDescriptorSetDeclaration(NAME("Material"));
    AssertThrow(decl != nullptr);

    DescriptorSetLayout layout { decl };

    FixedArray<DescriptorSetRef, maxFramesInFlight> descriptorSets;

    for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
    {
        DescriptorSetRef descriptorSet = g_renderBackend->MakeDescriptorSet(layout);

#ifdef HYP_DEBUG_MODE
        descriptorSet->SetDebugName(NAME_FMT("MaterialDescriptorSet_{}_{}", material->GetMaterial()->GetName(), frameIndex));
#endif

        for (uint32 textureIndex = 0; textureIndex < maxBoundTextures; textureIndex++)
        {
            descriptorSet->SetElement(NAME("Textures"), textureIndex, g_renderGlobalState->PlaceholderData->GetImageView2D1x1R8());
        }

        descriptorSets[frameIndex] = std::move(descriptorSet);
    }

    // if on render thread, initialize and add immediately
    if (Threads::IsOnThread(g_renderThread))
    {
        for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
        {
            HYPERION_ASSERT_RESULT(descriptorSets[frameIndex]->Create());
        }

        m_materialDescriptorSets.Insert(material, descriptorSets);
    }
    else
    {
        Mutex::Guard guard(m_pendingMutex);

        m_pendingAddition.PushBack({ material,
            descriptorSets });

        m_pendingAdditionFlag.Set(true, MemoryOrder::RELEASE);
    }

    return descriptorSets;
}

FixedArray<DescriptorSetRef, maxFramesInFlight> MaterialDescriptorSetManager::AddMaterial(RenderMaterial* material, FixedArray<Handle<Texture>, maxBoundTextures>&& textures)
{
    if (!material)
    {
        return {};
    }

    const DescriptorSetDeclaration* decl = g_renderGlobalState->GlobalDescriptorTable->GetDeclaration()->FindDescriptorSetDeclaration(NAME("Material"));
    AssertThrow(decl != nullptr);

    const DescriptorSetLayout layout { decl };

    FixedArray<DescriptorSetRef, maxFramesInFlight> descriptorSets;

    for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
    {
        DescriptorSetRef descriptorSet = g_renderBackend->MakeDescriptorSet(layout);

#ifdef HYP_DEBUG_MODE
        descriptorSet->SetDebugName(NAME_FMT("MaterialDescriptorSet_{}_{}", material->GetMaterial()->GetName(), frameIndex));
#endif

        for (uint32 textureIndex = 0; textureIndex < maxBoundTextures; textureIndex++)
        {
            if (textureIndex < textures.Size())
            {
                const Handle<Texture>& texture = textures[textureIndex];

                if (texture.IsValid() && texture->GetRenderResource().GetImageView() != nullptr)
                {
                    descriptorSet->SetElement(NAME("Textures"), textureIndex, texture->GetRenderResource().GetImageView());

                    continue;
                }
            }

            descriptorSet->SetElement(NAME("Textures"), textureIndex, g_renderGlobalState->PlaceholderData->GetImageView2D1x1R8());
        }

        descriptorSets[frameIndex] = std::move(descriptorSet);
    }

    // if on render thread, initialize and add immediately
    if (Threads::IsOnThread(g_renderThread))
    {
        for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
        {
            HYPERION_ASSERT_RESULT(descriptorSets[frameIndex]->Create());
        }

        m_materialDescriptorSets.Insert(material, descriptorSets);
    }
    else
    {
        Mutex::Guard guard(m_pendingMutex);

        m_pendingAddition.PushBack({ material,
            std::move(descriptorSets) });

        m_pendingAdditionFlag.Set(true, MemoryOrder::RELEASE);
    }

    return descriptorSets;
}

void MaterialDescriptorSetManager::RemoveMaterial(const RenderMaterial* material)
{
    Threads::AssertOnThread(g_renderThread);

    if (!material)
    {
        return;
    }

    { // remove from pending
        Mutex::Guard guard(m_pendingMutex);

        while (true)
        {
            const auto pendingAdditionIt = m_pendingAddition.FindIf([material](const auto& item)
                {
                    return item.first == material;
                });

            if (pendingAdditionIt == m_pendingAddition.End())
            {
                break;
            }

            m_pendingAddition.Erase(pendingAdditionIt);
        }

        const auto pendingRemovalIt = m_pendingRemoval.FindAs(material);

        if (pendingRemovalIt != m_pendingRemoval.End())
        {
            m_pendingRemoval.Erase(pendingRemovalIt);
        }
    }

    const auto materialDescriptorSetsIt = m_materialDescriptorSets.FindAs(material);

    if (materialDescriptorSetsIt != m_materialDescriptorSets.End())
    {
        for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
        {
            SafeRelease(std::move(materialDescriptorSetsIt->second[frameIndex]));
        }

        m_materialDescriptorSets.Erase(materialDescriptorSetsIt);
    }
}

void MaterialDescriptorSetManager::Initialize()
{
    CreateInvalidMaterialDescriptorSet();
}

void MaterialDescriptorSetManager::UpdatePendingDescriptorSets(FrameBase* frame)
{
    Threads::AssertOnThread(g_renderThread);

    const uint32 frameIndex = frame->GetFrameIndex();

    if (!m_pendingAdditionFlag.Get(MemoryOrder::ACQUIRE))
    {
        return;
    }

    Mutex::Guard guard(m_pendingMutex);

    for (auto it = m_pendingRemoval.Begin(); it != m_pendingRemoval.End();)
    {
        const auto materialDescriptorSetsIt = m_materialDescriptorSets.FindAs(*it);

        if (materialDescriptorSetsIt != m_materialDescriptorSets.End())
        {
            for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
            {
                SafeRelease(std::move(materialDescriptorSetsIt->second[frameIndex]));
            }

            m_materialDescriptorSets.Erase(materialDescriptorSetsIt);
        }

        it = m_pendingRemoval.Erase(it);
    }

    for (auto it = m_pendingAddition.Begin(); it != m_pendingAddition.End();)
    {
        for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
        {
            AssertThrow(it->second[frameIndex].IsValid());

            HYPERION_ASSERT_RESULT(it->second[frameIndex]->Create());
        }

        m_materialDescriptorSets.Insert(it->first, std::move(it->second));

        it = m_pendingAddition.Erase(it);
    }

    m_pendingAdditionFlag.Set(false, MemoryOrder::RELEASE);
}

void MaterialDescriptorSetManager::Update(FrameBase* frame)
{
    Threads::AssertOnThread(g_renderThread);

    const uint32 frameIndex = frame->GetFrameIndex();

    const uint32 descriptorSetsToUpdateFlag = m_descriptorSetsToUpdateFlag.Get(MemoryOrder::ACQUIRE);

    if (descriptorSetsToUpdateFlag & (1u << frameIndex))
    {
        Mutex::Guard guard(m_descriptorSetsToUpdateMutex);

        for (RenderMaterial* material : m_descriptorSetsToUpdate[frameIndex])
        {
            const auto it = m_materialDescriptorSets.Find(material);

            if (it == m_materialDescriptorSets.End())
            {
                continue;
            }

            AssertThrow(it->second[frameIndex].IsValid());
            it->second[frameIndex]->Update();
        }

        m_descriptorSetsToUpdate[frameIndex].Clear();

        m_descriptorSetsToUpdateFlag.BitAnd(~(1u << frameIndex), MemoryOrder::RELEASE);
    }
}

#pragma endregion MaterialDescriptorSetManager

HYP_DESCRIPTOR_SSBO_COND(Object, MaterialsBuffer, 1, ~0u, false, !g_renderBackend->GetRenderConfig().ShouldCollectUniqueDrawCallPerMaterial());
HYP_DESCRIPTOR_SSBO_COND(Object, MaterialsBuffer, 1, sizeof(MaterialShaderData), true, g_renderBackend->GetRenderConfig().ShouldCollectUniqueDrawCallPerMaterial());

} // namespace hyperion
