#ifndef HYPERION_RENDERER_DESCRIPTOR_POOL_H
#define HYPERION_RENDERER_DESCRIPTOR_POOL_H

#include "renderer_result.h"

#include "renderer_descriptor_set.h"

#include <vulkan/vulkan.h>

#include <unordered_map>
#include <memory>

namespace hyperion {
namespace renderer {
class Device;

class DescriptorPool {
    friend class DescriptorSet;
public:
    static const std::unordered_map<VkDescriptorType, size_t> items_per_set;
    static constexpr size_t max_descriptor_sets = 4;

    struct BufferInfo {
        VkDescriptorImageInfo *image_info;
        VkDescriptorBufferInfo *buffer_info;

        BufferInfo()
            : image_info(nullptr), buffer_info(nullptr) {}
        BufferInfo(VkDescriptorImageInfo *image_info, VkDescriptorBufferInfo *buffer_info)
            : image_info(image_info), buffer_info(buffer_info) {}
        BufferInfo(const BufferInfo &other)
            : image_info(other.image_info), buffer_info(other.buffer_info) {}
        inline BufferInfo &operator=(const BufferInfo &other)
        {
            image_info = other.image_info; 
            buffer_info = other.buffer_info;

            return *this;
        }
    };

    DescriptorPool();
    DescriptorPool(const DescriptorPool &other) = delete;
    DescriptorPool &operator=(const DescriptorPool &other) = delete;
    ~DescriptorPool();

    Result Create(Device *device);
    Result Destroy(Device *device);
    Result BindDescriptorSets(VkCommandBuffer cmd, VkPipelineLayout layout, size_t start_index, size_t size);
    Result BindDescriptorSets(VkCommandBuffer cmd, VkPipelineLayout layout);

    // return new descriptor set
    DescriptorSet &AddDescriptorSet();
    inline DescriptorSet *GetDescriptorSet(DescriptorSet::Index index)
        { return m_descriptor_sets[index].get(); }
    inline const DescriptorSet *GetDescriptorSet(DescriptorSet::Index index) const
        { return m_descriptor_sets[index].get(); }

    std::array<std::unique_ptr<DescriptorSet>, max_descriptor_sets> m_descriptor_sets;
    uint8_t m_num_descriptor_sets;
    std::vector<VkDescriptorSetLayout> m_descriptor_set_layouts;
    VkDescriptorPool m_descriptor_pool;
    VkDescriptorSet *m_descriptor_sets_view; // TMP

private:

    Result AllocateDescriptorSet(Device *device, VkDescriptorSetLayout *layout, DescriptorSet *out);
    Result CreateDescriptorSetLayout(Device *device, VkDescriptorSetLayoutCreateInfo *layout_create_info, VkDescriptorSetLayout *out);
    Result DestroyDescriptorSetLayout(Device *device, VkDescriptorSetLayout *layout);

};

} // namespace renderer
} // namespace hyperion

#endif