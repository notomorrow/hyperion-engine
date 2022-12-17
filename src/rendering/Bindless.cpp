#include "Bindless.hpp"

#include <Engine.hpp>

namespace hyperion::v2 {

/* The index of the descriptor we work on, /within/ the "bindless descriptor set" */
static const UInt bindless_descriptor_index = 0;

BindlessStorage::BindlessStorage()
    : m_descriptor_sets { }
{
}

BindlessStorage::~BindlessStorage() = default;

void BindlessStorage::Create()
{
    Threads::AssertOnThread(THREAD_RENDER);

    m_descriptor_sets[0] = Engine::Get()->GetGPUInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS);
    m_descriptor_sets[1] = Engine::Get()->GetGPUInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS_FRAME_1);
}

void BindlessStorage::Destroy()
{
    Threads::AssertOnThread(THREAD_RENDER);

    /* Remove all texture sub-descriptors */
    for (auto *descriptor_set : m_descriptor_sets) {
        auto *descriptor = descriptor_set->GetDescriptor(bindless_descriptor_index);

        for (const auto &it : m_texture_ids) {
            descriptor->RemoveSubDescriptor(it.first.ToIndex());
        }
    }

    m_texture_ids.Clear();
}

void BindlessStorage::AddResource(Texture *texture)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertThrow(texture != nullptr);

    for (SizeType i = 0; i < m_descriptor_sets.Size(); i++) {
        auto *descriptor_set = m_descriptor_sets[i];
        auto *descriptor = descriptor_set->GetDescriptor(bindless_descriptor_index);
        
        descriptor->SetElementImageSamplerCombined(
            texture->GetID().ToIndex(),
            texture->GetImageView(),
            texture->GetSampler()
        );
    }
    
    const auto insert_result = m_texture_ids.Insert(texture->GetID(), texture);

    AssertThrowMsg(insert_result.second, "Duplicate AddResource call for index %u!", texture->GetID().ToIndex());
}

void BindlessStorage::RemoveResource(ID<Texture> id)
{
    Threads::AssertOnThread(THREAD_RENDER);

    if (!id) {
        return;
    }

    for (auto *descriptor_set : m_descriptor_sets) {
        auto *descriptor = descriptor_set->GetDescriptor(bindless_descriptor_index);

        descriptor->SetElementImageSamplerCombined(
            id.ToIndex(),
            &Engine::Get()->GetPlaceholderData().GetImageView2D1x1R8(),
            &Engine::Get()->GetPlaceholderData().GetSamplerLinear()
        );
    }

    m_texture_ids.Erase(id);
}

void BindlessStorage::MarkResourceChanged(ID<Texture> id)
{
    Threads::AssertOnThread(THREAD_RENDER);

    if (!id) {
        return;
    }

    for (auto *descriptor_set : m_descriptor_sets) {
        descriptor_set->GetDescriptor(bindless_descriptor_index)->MarkDirty(id.ToIndex());
    }
}

Texture *BindlessStorage::GetResource(ID<Texture> id) const
{
    Threads::AssertOnThread(THREAD_RENDER);

    if (!id) {
        return nullptr;
    }

    const auto it = m_texture_ids.Find(id);

    if (it == m_texture_ids.End()) {
        return nullptr;
    }

    return it->second;
}

} // namespace hyperion::v2
