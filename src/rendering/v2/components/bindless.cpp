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
        auto *descriptor = descriptor_set->GetDescriptor(0);

        for (const size_t sub_descriptor_index : m_texture_sub_descriptors) {
            descriptor->RemoveSubDescriptor(sub_descriptor_index);
        }
    }
}

void BindlessStorage::ApplyUpdates(Engine *engine, uint32_t frame_index)
{
    auto *descriptor_set = m_descriptor_sets[frame_index % m_descriptor_sets.size()];

    descriptor_set->ApplyUpdates(engine->GetInstance()->GetDevice());
}

void BindlessStorage::AddResource(Texture *texture)
{
    AssertThrow(texture != nullptr);
    AssertThrow(texture->GetImageView() != nullptr);
    AssertThrow(texture->GetSampler() != nullptr);

    size_t indices[2] = {};
    
    for (size_t i = 0; i < m_descriptor_sets.size(); i++) {
        auto *descriptor_set = m_descriptor_sets[i];
        auto *descriptor = descriptor_set->GetDescriptor(0);

        indices[i] = descriptor->AddSubDescriptor({
            .image_view = texture->GetImageView(),
            .sampler    = texture->GetSampler()
        });
    }

    AssertThrow(indices[0] == indices[1]);

    m_texture_sub_descriptors.Set(texture->GetId(), std::move(indices[0]));
}

void BindlessStorage::RemoveResource(Texture *texture)
{
    AssertThrow(texture != nullptr);

    const size_t sub_descriptor_index = m_texture_sub_descriptors.Get(texture->GetId());
    
    for (auto *descriptor_set : m_descriptor_sets) {
        auto *descriptor = descriptor_set->GetDescriptor(0);

        descriptor->RemoveSubDescriptor(sub_descriptor_index);
    }

    m_texture_sub_descriptors.Remove(texture->GetId());
}

void BindlessStorage::MarkResourceChanged(Texture *texture)
{
    const auto id_value = texture->GetId().Value() - 1;
    const size_t sub_descriptor_index = m_texture_sub_descriptors.Get(texture->GetId());

    for (auto *descriptor_set : m_descriptor_sets) {
        descriptor_set->GetDescriptor(0)->MarkDirty(sub_descriptor_index);
    }
}

} // namespace hyperion::v2