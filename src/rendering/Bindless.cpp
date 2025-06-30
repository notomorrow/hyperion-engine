/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Bindless.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderGlobalState.hpp>

#include <Engine.hpp>

namespace hyperion {

BindlessStorage::BindlessStorage() = default;
BindlessStorage::~BindlessStorage() = default;

void BindlessStorage::Create()
{
    Threads::AssertOnThread(g_renderThread);
}

void BindlessStorage::Destroy()
{
    Threads::AssertOnThread(g_renderThread);

    for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = g_renderGlobalState->GlobalDescriptorTable->GetDescriptorSet(NAME("Material"), frameIndex);
        AssertDebug(descriptorSet.IsValid());

        for (const auto& it : m_resources)
        {
            descriptorSet->SetElement(NAME("Textures"), it.first.ToIndex(), g_renderGlobalState->PlaceholderData->GetImageView2D1x1R8());
        }
    }

    m_resources.Clear();
}

void BindlessStorage::AddResource(ObjId<Texture> id, const ImageViewRef& imageView)
{
    Threads::AssertOnThread(g_renderThread);

    if (!id.IsValid())
    {
        return;
    }

    auto it = m_resources.Find(id);

    if (it != m_resources.End())
    {
        return;
    }

    m_resources.Insert({ id, imageView });

    for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = g_renderGlobalState->GlobalDescriptorTable->GetDescriptorSet(NAME("Material"), frameIndex);
        AssertDebug(descriptorSet.IsValid());

        descriptorSet->SetElement(NAME("Textures"), id.ToIndex(), imageView);
    }
}

void BindlessStorage::RemoveResource(ObjId<Texture> id)
{
    Threads::AssertOnThread(g_renderThread);

    if (!id.IsValid())
    {
        return;
    }

    auto it = m_resources.Find(id);

    if (it == m_resources.End())
    {
        return;
    }

    m_resources.Erase(it);

    for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = g_renderGlobalState->GlobalDescriptorTable->GetDescriptorSet(NAME("Material"), frameIndex);
        AssertDebug(descriptorSet.IsValid());

        descriptorSet->SetElement(NAME("Textures"), id.ToIndex(), g_renderGlobalState->PlaceholderData->GetImageView2D1x1R8());
    }
}

} // namespace hyperion
