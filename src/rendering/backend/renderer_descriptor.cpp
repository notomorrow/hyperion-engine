#include "renderer_descriptor_set.h"
#include "renderer_device.h"
#include "../../system/debug.h"

namespace hyperion {
namespace renderer {
Descriptor::Descriptor(uint32_t binding, Mode mode, VkShaderStageFlags stage_flags)
    : m_binding(binding),
      m_mode(mode),
      m_stage_flags(stage_flags),
      m_state(DESCRIPTOR_DIRTY)
{
}

Descriptor::~Descriptor()
{
}

void Descriptor::Create(Device *device, Descriptor::Info *out_info)
{
    uint32_t num_descriptors = 0;

    for (auto &sub : m_sub_descriptors) {
        switch (m_mode) {
        case Mode::UNIFORM_BUFFER: /* fallthrough */
        case Mode::UNIFORM_BUFFER_DYNAMIC:
            AssertThrow(sub.gpu_buffer != nullptr);
            AssertThrow(sub.gpu_buffer->buffer != nullptr);

            m_sub_descriptor_buffer.buffers.push_back(VkDescriptorBufferInfo{
                .buffer = sub.gpu_buffer->buffer,
                .offset = 0,
                .range = sub.range ? sub.range : sub.gpu_buffer->size
            });

            ++num_descriptors;

            break;
        case Mode::IMAGE_SAMPLER:
            AssertThrow(sub.image_view != nullptr);
            AssertThrow(sub.image_view->GetImageView() != nullptr);

            AssertThrow(sub.sampler != nullptr);
            AssertThrow(sub.sampler->GetSampler() != nullptr);

            m_sub_descriptor_buffer.images.push_back(VkDescriptorImageInfo{
                .sampler = sub.sampler->GetSampler(),
                .imageView = sub.image_view->GetImageView(),
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            });

            ++num_descriptors;

            break;
        case Mode::IMAGE_STORAGE:
            AssertThrow(sub.image_view != nullptr);
            AssertThrow(sub.image_view->GetImageView() != nullptr);

            m_sub_descriptor_buffer.images.push_back(VkDescriptorImageInfo{
                .sampler = nullptr,
                .imageView = sub.image_view->GetImageView(),
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL
            });

            ++num_descriptors;


            break;
        }
    }

    const auto descriptor_type = GetDescriptorType(m_mode);

    out_info->binding = VkDescriptorSetLayoutBinding{};
    out_info->binding.descriptorCount = num_descriptors;
    out_info->binding.descriptorType = descriptor_type;
    out_info->binding.pImmutableSamplers = nullptr;
    out_info->binding.stageFlags = m_stage_flags;
    out_info->binding.binding = m_binding;

    out_info->write = VkWriteDescriptorSet{};
    out_info->write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    out_info->write.pNext = nullptr;
    out_info->write.descriptorCount = num_descriptors;
    out_info->write.descriptorType = descriptor_type;
    out_info->write.pBufferInfo = m_sub_descriptor_buffer.buffers.data();
    out_info->write.pImageInfo = m_sub_descriptor_buffer.images.data();
    out_info->write.dstBinding = m_binding;
}

void Descriptor::Destroy(Device *device)
{
}

VkDescriptorType Descriptor::GetDescriptorType(Mode mode)
{
    switch (mode) {
    case Mode::UNIFORM_BUFFER: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case Mode::UNIFORM_BUFFER_DYNAMIC: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    case Mode::IMAGE_SAMPLER: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    case Mode::IMAGE_STORAGE: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    default:
        AssertThrowMsg(false, "Unsupported descriptor type");
    }
}

} // namespace renderer
} // namespace hyperion