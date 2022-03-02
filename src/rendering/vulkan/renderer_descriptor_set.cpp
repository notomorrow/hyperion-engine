#include "renderer_descriptor_set.h"
#include "renderer_descriptor_pool.h"

namespace hyperion {

RendererDescriptorSet::RendererDescriptorSet()
{
}

RendererDescriptorSet::~RendererDescriptorSet()
{
}

RendererDescriptor *RendererDescriptorSet::AddDescriptor(uint32_t binding,
    size_t size,
    VkDescriptorType type,
    VkBufferUsageFlags usage_flags,
    VkShaderStageFlags stage_flags)
{
    m_descriptors.emplace_back(std::make_unique<RendererDescriptor>(
        binding,
        size,
        type,
        usage_flags,
        stage_flags
    ));

    return m_descriptors.back().get();
}

RendererResult RendererDescriptorSet::Create(RendererDevice *device, RendererDescriptorPool *pool)
{
    std::vector<VkWriteDescriptorSet> writes;
    writes.reserve(m_descriptors.size());

    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.reserve(m_descriptors.size());

    for (auto &descriptor : m_descriptors) {
        descriptor->Create(device);

        VkDescriptorSetLayoutBinding new_binding{};
        new_binding.descriptorCount = 1;
        new_binding.descriptorType = descriptor->m_type;
        new_binding.pImmutableSamplers = nullptr;
        new_binding.stageFlags = descriptor->m_stage_flags;
        new_binding.binding = descriptor->m_binding;

        bindings.push_back(new_binding);

        VkWriteDescriptorSet new_write{};
        new_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        new_write.pNext = nullptr;
        new_write.descriptorCount = 1;
        new_write.descriptorType = descriptor->m_type;
        new_write.pBufferInfo = &descriptor->m_buffer_info;
        new_write.pImageInfo = &descriptor->m_image_info;
        new_write.dstBinding = descriptor->m_binding;

        writes.push_back(new_write);
    }

    //build layout first
    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.pNext = nullptr;
    layout_info.pBindings = bindings.data();
    layout_info.bindingCount = uint32_t(bindings.size());

    VkDescriptorSetLayout layout;

    {
        auto layout_result = pool->CreateDescriptorSetLayout(device, &layout_info, &layout);

        if (!layout_result) {
            DebugLog(LogType::Error, "Failed to create descriptor set layout! Message was: %s\n", layout_result.message);

            return layout_result;
        }
    }

    {
        auto allocate_result = pool->AllocateDescriptorSet(device, &layout, this);

        if (!allocate_result) {
            DebugLog(LogType::Error, "Failed to allocate descriptor set! Message was: %s\n", allocate_result.message);

            return allocate_result;
        }
    }

    //write descriptor
    for (VkWriteDescriptorSet &w : writes) {
        w.dstSet = m_set;
    }

    vkUpdateDescriptorSets(device->GetDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    return RendererResult(RendererResult::RENDERER_OK);
}

RendererResult RendererDescriptorSet::Destroy(RendererDevice *device)
{
    for (auto &descriptor : m_descriptors) {
        descriptor->Destroy(device);
    }


    // TODO: clear descriptor set

    return RendererResult(RendererResult::RENDERER_OK);
}

} // namespace hyperion