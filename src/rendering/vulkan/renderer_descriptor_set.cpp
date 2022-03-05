#include "renderer_descriptor_set.h"
#include "renderer_descriptor_pool.h"

namespace hyperion {

RendererDescriptorSet::RendererDescriptorSet()
{
}

RendererDescriptorSet::~RendererDescriptorSet()
{
}

RendererDescriptorSet &RendererDescriptorSet::AddDescriptor(std::unique_ptr<RendererDescriptor> &&descriptor)
{
    m_descriptors.emplace_back(std::move(descriptor));

    return *this;
}

RendererResult RendererDescriptorSet::Create(RendererDevice *device, RendererDescriptorPool *pool)
{
    std::vector<VkWriteDescriptorSet> writes;
    writes.reserve(m_descriptors.size());

    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.reserve(m_descriptors.size());

    for (auto &descriptor : m_descriptors) {
        RendererDescriptor::Info info{};

        descriptor->Create(device, &info);

        bindings.push_back(info.binding);
        writes.push_back(info.write);
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