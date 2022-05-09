#include "bindless.h"

#include "rendering/v2/engine.h"

namespace hyperion::v2 {

BindlessStorage::BindlessStorage()
    : m_descriptor_sets{}
{
}

BindlessStorage::~BindlessStorage() = default;

void BindlessStorage::Create(Engine *engine)
{
    m_descriptor_sets[0] = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS);
    m_descriptor_sets[1] = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS_FRAME_1);
}

void BindlessStorage::Destroy(Engine *engine)
{
    m_textures_pending_addition.clear();

    RemoveEnqueued();

    /* Remove all texture sub-descriptors */
    for (auto *descriptor_set : m_descriptor_sets) {
        auto *descriptor = descriptor_set->GetDescriptor(bindless_descriptor_index);

        for (const auto &it : m_texture_resources) {
            descriptor->RemoveSubDescriptor(it.second.resource_index);
        }
    }

    m_texture_resources.clear();
}

void BindlessStorage::ApplyUpdates(Engine *engine, uint32_t frame_index)
{
    if (m_is_pending_changes) {
        std::lock_guard guard(m_enqueued_resources_mutex);

        if (!m_textures_pending_addition.empty()) {
            AddEnqueued();
        }

        if (!m_textures_pending_removal.empty()) {
            RemoveEnqueued();
        }

        m_is_pending_changes = false;
    }

    auto *descriptor_set = m_descriptor_sets[frame_index % m_descriptor_sets.size()];

    descriptor_set->ApplyUpdates(engine->GetInstance()->GetDevice());
}

void BindlessStorage::AddEnqueued()
{
    for (size_t j = 0; j < m_textures_pending_addition.size(); j++) {
        auto &texture = m_textures_pending_addition[j];
        uint32_t indices[2] = {};

        for (size_t i = 0; i < m_descriptor_sets.size(); i++) {
            auto *descriptor_set = m_descriptor_sets[i];
            auto *descriptor = descriptor_set->GetDescriptor(bindless_descriptor_index);


            indices[i] = descriptor->AddSubDescriptor({
                .image_view = texture->GetImageView(),
                .sampler    = texture->GetSampler()
            });
        }

        AssertThrow(indices[0] == indices[1]);

        m_texture_resources[texture->GetId().value] = {texture.ptr, indices[0]};
    }

    m_textures_pending_addition.clear();
}

void BindlessStorage::RemoveEnqueued()
{
    for (size_t i = 0; i < m_textures_pending_removal.size(); i++) {
        auto &pending_removal = m_textures_pending_removal[i];

        const auto it = m_texture_resources.find(pending_removal.value);

        if (it == m_texture_resources.end()) {
            DebugLog(
                LogType::Warn,
                "Attempt to remove texture with id #%lu but could not be found\n",
                pending_removal.value
            );

            continue;
        }

        const auto resource_index = it->second.resource_index;

        for (auto *descriptor_set : m_descriptor_sets) {
            auto *descriptor = descriptor_set->GetDescriptor(bindless_descriptor_index);

            descriptor->RemoveSubDescriptor(resource_index);
        }

        m_texture_resources.erase(it);
        
        /* Have to update indices of all other known subdescriptor indices that are
         * greater than `sub_descriptor_index`, as the indices would have changed
         * TODO: Use a sorted vector or something, maintained with upper_bound
         */

        for (auto &res : m_texture_resources) {
            if (res.second.resource_index > resource_index && res.second.resource_index != UINT32_MAX) {
                --res.second.resource_index;
            }
        }
    }

    m_textures_pending_removal.clear();
}

void BindlessStorage::AddResource(Ref<Texture> &&texture)
{
    AssertThrow(texture != nullptr);
    AssertThrow(texture->GetImageView() != nullptr);
    AssertThrow(texture->GetSampler() != nullptr);

    /*AssertThrowMsg(
        m_texture_sub_descriptors.Size() < DescriptorSet::max_bindless_resources,
        "Number of bound textures must not exceed limit of %ul",
        DescriptorSet::max_bindless_resources
    );*/

    std::lock_guard guard(m_enqueued_resources_mutex);
    m_textures_pending_addition.push_back(std::move(texture));
    m_is_pending_changes = true;
}

void BindlessStorage::RemoveResource(Texture::ID id)
{
    std::lock_guard guard(m_enqueued_resources_mutex);
    m_textures_pending_removal.push_back(id);
    m_is_pending_changes = true;
}

void BindlessStorage::MarkResourceChanged(const Texture *texture)
{
    const auto sub_descriptor_index = m_texture_resources[texture->GetId().value].resource_index;

    for (auto *descriptor_set : m_descriptor_sets) {
        descriptor_set->GetDescriptor(bindless_descriptor_index)->MarkDirty(sub_descriptor_index);
    }
}

bool BindlessStorage::GetResourceIndex(const Texture *texture, uint32_t *out_index) const
{
    return GetResourceIndex(texture->GetId(), out_index);
}

bool BindlessStorage::GetResourceIndex(Texture::ID id, uint32_t *out_index) const
{
    auto it = m_texture_resources.find(id.value);

    if (it == m_texture_resources.end()) {
        return false;
    }

    *out_index = it->second.resource_index;

    return true;
}

} // namespace hyperion::v2