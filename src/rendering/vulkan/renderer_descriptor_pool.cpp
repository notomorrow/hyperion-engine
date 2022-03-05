#include "renderer_descriptor_pool.h"
#include "renderer_device.h"
#include "renderer_descriptor_set.h"
#include "../../util.h"

#include <vector>

namespace hyperion {

const std::unordered_map<VkDescriptorType, size_t> RendererDescriptorPool::items_per_set{
    { VK_DESCRIPTOR_TYPE_SAMPLER, 1 },
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 },
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 2 },
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 }
};

const size_t RendererDescriptorPool::max_descriptor_sets = 4;

RendererDescriptorPool::RendererDescriptorPool()
    : m_descriptor_pool(nullptr),
      m_descriptor_sets_view(new VkDescriptorSet[max_descriptor_sets])
{
}

RendererDescriptorPool::~RendererDescriptorPool()
{
    AssertExitMsg(m_descriptor_pool == nullptr, "descriptor pool should have been destroyed!");

    delete[] m_descriptor_sets_view;
}

RendererDescriptorSet &RendererDescriptorPool::AddDescriptorSet()
{
    AssertThrowMsg(m_descriptor_sets.size() + 1 <= max_descriptor_sets, "Maximum number of descriptor sets added");

    m_descriptor_sets.emplace_back(std::make_unique<RendererDescriptorSet>());

    return *m_descriptor_sets.back();
}

RendererResult RendererDescriptorPool::Create(RendererDevice *device, VkDescriptorPoolCreateFlags flags)
{
    uint32_t set_size = m_descriptor_sets.size();

    std::vector<VkDescriptorPoolSize> pool_sizes;
    pool_sizes.reserve(items_per_set.size());

    for (auto &it : RendererDescriptorPool::items_per_set) {
        pool_sizes.push_back({ it.first, uint32_t(it.second * set_size) });
    }

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = flags;
    pool_info.maxSets = set_size;
    pool_info.poolSizeCount = (uint32_t)pool_sizes.size();
    pool_info.pPoolSizes = pool_sizes.data();

    if (vkCreateDescriptorPool(device->GetDevice(), &pool_info, nullptr, &m_descriptor_pool) != VK_SUCCESS) {
        return RendererResult(RendererResult::RENDERER_ERR, "Could not create descriptor pool.");
    }

    size_t index = 0;

    for (const auto &set : m_descriptor_sets) {
        auto descriptor_set_result = set->Create(device, this);

        if (!descriptor_set_result) return descriptor_set_result;

        m_descriptor_sets_view[index++] = set->m_set;
    }

    return RendererResult(RendererResult::RENDERER_OK);
}

RendererResult RendererDescriptorPool::Destroy(RendererDevice *device)
{
    {
        for (auto &layout : m_descriptor_set_layouts) {
            vkDestroyDescriptorSetLayout(device->GetDevice(), layout, nullptr);
        }

        m_descriptor_set_layouts.clear();
    }

    {
        for (auto &set : m_descriptor_sets) {
            set->Destroy(device);
        }

        vkFreeDescriptorSets(device->GetDevice(), m_descriptor_pool, m_descriptor_sets.size(), m_descriptor_sets_view);

        // set all to nullptr
        std::memset(m_descriptor_sets_view, 0, sizeof(VkDescriptorSet *) * max_descriptor_sets);
    }

    {
        vkDestroyDescriptorPool(device->GetDevice(), m_descriptor_pool, nullptr);
        m_descriptor_pool = nullptr;
    }

    return RendererResult(RendererResult::RENDERER_OK);
}

RendererResult RendererDescriptorPool::BindDescriptorSets(VkCommandBuffer cmd, VkPipelineLayout layout, size_t start_index, size_t size)
{
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, size, &m_descriptor_sets_view[start_index], 0, nullptr);

    return RendererResult(RendererResult::RENDERER_OK);
}

RendererResult RendererDescriptorPool::BindDescriptorSets(VkCommandBuffer cmd, VkPipelineLayout layout)
{
    return BindDescriptorSets(cmd, layout, 0, m_descriptor_sets.size());
}


RendererResult RendererDescriptorPool::CreateDescriptorSetLayout(RendererDevice *device, VkDescriptorSetLayoutCreateInfo *layout_create_info, VkDescriptorSetLayout *out)
{
    // TODO: caching
    if (vkCreateDescriptorSetLayout(device->GetDevice(), layout_create_info, nullptr, out) != VK_SUCCESS) {
        return RendererResult(RendererResult::RENDERER_ERR, "Could not create descriptor set layout");
    }

    m_descriptor_set_layouts.push_back(*out);

    return RendererResult(RendererResult::RENDERER_OK);
}

RendererResult RendererDescriptorPool::DestroyDescriptorSetLayout(RendererDevice *device, VkDescriptorSetLayout *layout)
{
    auto it = std::find(m_descriptor_set_layouts.begin(), m_descriptor_set_layouts.end(), *layout);

    if (it == m_descriptor_set_layouts.end()) {
        return RendererResult(RendererResult::RENDERER_ERR, "Could not destroy descriptor set layout; not found in list");
    }

    vkDestroyDescriptorSetLayout(device->GetDevice(), *layout, nullptr);

    m_descriptor_set_layouts.erase(it);

    return RendererResult(RendererResult::RENDERER_OK);
}

RendererResult RendererDescriptorPool::AllocateDescriptorSet(RendererDevice *device, VkDescriptorSetLayout *layout, RendererDescriptorSet *out)
{
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.pNext = nullptr;
    alloc_info.pSetLayouts = layout;
    alloc_info.descriptorPool = m_descriptor_pool;
    alloc_info.descriptorSetCount = 1;

    VkResult alloc_result = vkAllocateDescriptorSets(device->GetDevice(), &alloc_info, &out->m_set);

    switch (alloc_result) {
    case VK_SUCCESS: return RendererResult(RendererResult::RENDERER_OK);
    case VK_ERROR_FRAGMENTED_POOL:
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        // TODO: re-allocation
        return RendererResult(RendererResult::RENDERER_ERR_NEEDS_REALLOCATION, "Needs reallocation");
    default:
        return RendererResult(RendererResult::RENDERER_ERR, "Unknown error");
    }
}

} // namespace hyperion