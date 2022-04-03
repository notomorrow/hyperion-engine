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
    /* no-op */
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

    const auto id_value = texture->GetId().Value() - 1;
    
    for (auto *descriptor_set : m_descriptor_sets) {
        auto *descriptor = descriptor_set->GetDescriptor(0);

        /* for now */
        AssertThrow(id_value == descriptor->GetSubDescriptors().size());

        descriptor->AddSubDescriptor({
            .image_view = texture->GetImageView(),
            .sampler    = texture->GetSampler()
        });
    }
}


void BindlessStorage::RemoveResource(Texture *texture)
{
    AssertThrow(texture != nullptr);

    const auto id_value = texture->GetId().Value() - 1;
    
    for (auto *descriptor_set : m_descriptor_sets) {
        auto *descriptor = descriptor_set->GetDescriptor(0);

        /* for now */
        AssertThrow(id_value < descriptor->GetSubDescriptors().size());

        descriptor->RemoveSubDescriptor(id_value);
    }
}

void BindlessStorage::MarkResourceChanged(Texture *texture)
{
    const auto id_value = texture->GetId().Value() - 1;

    for (auto *descriptor_set : m_descriptor_sets) {
        /* tmp */
        descriptor_set->GetDescriptor(0)->MarkDirty(id_value);
    }
}

} // namespace hyperion::v2