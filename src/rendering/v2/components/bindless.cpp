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
    /* Remove all texture sub-descriptors */
    for (auto *descriptor_set : m_descriptor_sets) {
        auto *descriptor = descriptor_set->GetDescriptor(bindless_descriptor_index);

        for (const auto &it : m_texture_sub_descriptors) {
            descriptor->RemoveSubDescriptor(it.second);
        }
    }

    m_texture_sub_descriptors.Clear();
}

void BindlessStorage::ApplyUpdates(Engine *engine, uint32_t frame_index)
{    
    //engine->texture_mutex.lock();
    auto *descriptor_set = m_descriptor_sets[frame_index % m_descriptor_sets.size()];

    descriptor_set->ApplyUpdates(engine->GetInstance()->GetDevice());
    //engine->texture_mutex.unlock();
}

void BindlessStorage::AddResource(const Texture *texture)
{
    AssertThrow(texture != nullptr);
    AssertThrow(texture->GetImageView() != nullptr);
    AssertThrow(texture->GetSampler() != nullptr);

    AssertThrowMsg(
        m_texture_sub_descriptors.Size() < DescriptorSet::max_bindless_resources,
        "Number of bound textures must not exceed limit of %ul",
        DescriptorSet::max_bindless_resources
    );

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

    m_texture_sub_descriptors.Set(texture->GetId(), std::move(indices[0]));
}

void BindlessStorage::RemoveResource(const Texture *texture)
{
    AssertThrow(texture != nullptr);
    AssertThrow(m_texture_sub_descriptors.Has(texture->GetId()));

    const auto sub_descriptor_index = m_texture_sub_descriptors[texture->GetId()];
    
    for (auto *descriptor_set : m_descriptor_sets) {
        auto *descriptor = descriptor_set->GetDescriptor(bindless_descriptor_index);

        descriptor->RemoveSubDescriptor(sub_descriptor_index);
    }

    m_texture_sub_descriptors.Remove(texture->GetId());

    /* Have to update indices of all other known subdescriptor indices that are
     * greater than `sub_descriptor_index`, as the indices would have changed
     * TODO: Use a sorted vector or something, maintained with upper_bound
     */

    for (auto &it : m_texture_sub_descriptors) {
        if (it.second > sub_descriptor_index) {
            --it.second;
        }
    }
}

void BindlessStorage::MarkResourceChanged(const Texture *texture)
{
    const auto sub_descriptor_index = m_texture_sub_descriptors[texture->GetId()];

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
    if (!m_texture_sub_descriptors.Has(id)) {
        return false;
    }

    *out_index = m_texture_sub_descriptors.Get(id);

    return true;
}

} // namespace hyperion::v2