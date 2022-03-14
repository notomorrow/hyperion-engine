#include "renderer_descriptor_pool.h"
#include "renderer_device.h"
#include "renderer_descriptor_set.h"
#include "../../util.h"

#include <vector>

namespace hyperion {
namespace renderer {
const std::unordered_map<VkDescriptorType, size_t> DescriptorPool::items_per_set{
    { VK_DESCRIPTOR_TYPE_SAMPLER, 10 },
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 40 }, /* sampling imageviews in shader */
    { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10 },          /* imageStore, imageLoad etc */
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 20 },         /* standard uniform buffer */
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 20 },
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 20 }

};

DescriptorPool::DescriptorPool()
    : m_descriptor_pool(nullptr),
      m_num_descriptor_sets(0),
      m_descriptor_sets_view(new VkDescriptorSet[max_descriptor_sets])
{
}

DescriptorPool::~DescriptorPool()
{
    AssertExitMsg(m_descriptor_pool == nullptr, "descriptor pool should have been destroyed!");

    delete[] m_descriptor_sets_view;
}

DescriptorSet &DescriptorPool::AddDescriptorSet()
{
    AssertThrowMsg(m_num_descriptor_sets + 1 <= max_descriptor_sets, "Maximum number of descriptor sets added");

    m_descriptor_sets[m_num_descriptor_sets++] = std::make_unique<DescriptorSet>();

    return *m_descriptor_sets[m_num_descriptor_sets - 1];
}

Result DescriptorPool::Create(Device *device)
{
    std::vector<VkDescriptorPoolSize> pool_sizes;
    pool_sizes.reserve(items_per_set.size());

    for (auto &it : items_per_set) {
        pool_sizes.push_back({ it.first, uint32_t(it.second * max_descriptor_sets) });
    }

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = max_descriptor_sets;
    pool_info.poolSizeCount = uint32_t(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();

    if (vkCreateDescriptorPool(device->GetDevice(), &pool_info, nullptr, &m_descriptor_pool) != VK_SUCCESS) {
        return Result(Result::RENDERER_ERR, "Could not create descriptor pool.");
    }

    size_t index = 0;

    for (uint8_t i = 0; i < m_num_descriptor_sets; i++) {
        AssertThrow(m_descriptor_sets[i] != nullptr);

        auto descriptor_set_result = m_descriptor_sets[i]->Create(device, this);

        if (!descriptor_set_result) return descriptor_set_result;

        m_descriptor_sets_view[index++] = m_descriptor_sets[i]->m_set;
    }

    HYPERION_RETURN_OK;
}

Result DescriptorPool::Destroy(Device *device)
{
    {
        for (auto &layout : m_descriptor_set_layouts) {
            vkDestroyDescriptorSetLayout(device->GetDevice(), layout, nullptr);
        }

        m_descriptor_set_layouts.clear();
    }

    {
        for (auto &set : m_descriptor_sets) {
            if (set != nullptr) {
                set->Destroy(device);
            }
        }

        vkFreeDescriptorSets(device->GetDevice(), m_descriptor_pool, m_num_descriptor_sets, m_descriptor_sets_view);

        m_descriptor_sets = {};

        // set all to nullptr
        std::memset(m_descriptor_sets_view, 0, sizeof(VkDescriptorSet *) * max_descriptor_sets);
    }

    {
        vkDestroyDescriptorPool(device->GetDevice(), m_descriptor_pool, nullptr);
        m_descriptor_pool = nullptr;
    }

    HYPERION_RETURN_OK;
}

Result DescriptorPool::BindDescriptorSets(VkCommandBuffer cmd, VkPipelineLayout layout, size_t start_index, size_t size)
{
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, size, &m_descriptor_sets_view[start_index], 0, nullptr);

    HYPERION_RETURN_OK;
}

Result DescriptorPool::BindDescriptorSets(VkCommandBuffer cmd, VkPipelineLayout layout)
{
    return BindDescriptorSets(cmd, layout, 0, m_num_descriptor_sets);
}


Result DescriptorPool::CreateDescriptorSetLayout(Device *device, VkDescriptorSetLayoutCreateInfo *layout_create_info, VkDescriptorSetLayout *out)
{
    // TODO: caching
    if (vkCreateDescriptorSetLayout(device->GetDevice(), layout_create_info, nullptr, out) != VK_SUCCESS) {
        return Result(Result::RENDERER_ERR, "Could not create descriptor set layout");
    }

    m_descriptor_set_layouts.push_back(*out);

    HYPERION_RETURN_OK;
}

Result DescriptorPool::DestroyDescriptorSetLayout(Device *device, VkDescriptorSetLayout *layout)
{
    auto it = std::find(m_descriptor_set_layouts.begin(), m_descriptor_set_layouts.end(), *layout);

    if (it == m_descriptor_set_layouts.end()) {
        return Result(Result::RENDERER_ERR, "Could not destroy descriptor set layout; not found in list");
    }

    vkDestroyDescriptorSetLayout(device->GetDevice(), *layout, nullptr);

    m_descriptor_set_layouts.erase(it);

    HYPERION_RETURN_OK;
}

Result DescriptorPool::AllocateDescriptorSet(Device *device, VkDescriptorSetLayout *layout, DescriptorSet *out)
{
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.pNext = nullptr;
    alloc_info.pSetLayouts = layout;
    alloc_info.descriptorPool = m_descriptor_pool;
    alloc_info.descriptorSetCount = 1;

    VkResult alloc_result = vkAllocateDescriptorSets(device->GetDevice(), &alloc_info, &out->m_set);

    switch (alloc_result) {
    case VK_SUCCESS: return Result::OK;
    case VK_ERROR_FRAGMENTED_POOL:
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        // TODO: re-allocation
        return Result(Result::RENDERER_ERR_NEEDS_REALLOCATION, "Needs reallocation");
    default:
        return Result(Result::RENDERER_ERR, "Unknown error");
    }
}

} // namespace renderer
} // namespace hyperion