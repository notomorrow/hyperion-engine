#ifndef HYPERION_RENDERER_DESCRIPTOR_POOL_H
#define HYPERION_RENDERER_DESCRIPTOR_POOL_H

#include "renderer_result.h"

#include <vulkan/vulkan.h>

#include <unordered_map>
#include <memory>

namespace hyperion {
class RendererDevice;

class RendererDescriptorPool {
    friend class RendererDescriptorSet;
public:
    static const std::unordered_map<VkDescriptorType, size_t> items_per_set;
    static const size_t max_descriptor_sets;

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

    RendererDescriptorPool();
    RendererDescriptorPool(const RendererDescriptorPool &other) = delete;
    RendererDescriptorPool &operator=(const RendererDescriptorPool &other) = delete;
    ~RendererDescriptorPool();

    RendererResult Create(RendererDevice *device, VkDescriptorPoolCreateFlags flags);
    RendererResult Destroy(RendererDevice *device);
    RendererResult BindDescriptorSets(VkCommandBuffer cmd, VkPipelineLayout layout);

    RendererDescriptorSet *AddDescriptorSet();
    inline RendererDescriptorSet *GetDescriptorSet(size_t index) { return m_descriptor_sets[index].get(); }
    inline const RendererDescriptorSet *GetDescriptorSet(size_t index) const { return m_descriptor_sets[index].get(); }

    std::vector<std::unique_ptr<RendererDescriptorSet>> m_descriptor_sets;
    std::vector<VkDescriptorSetLayout> m_descriptor_set_layouts;
    VkDescriptorPool m_descriptor_pool;

private:

    RendererResult AllocateDescriptorSet(RendererDevice *device, VkDescriptorSetLayout *layout, RendererDescriptorSet *out);
    RendererResult CreateDescriptorSetLayout(RendererDevice *device, VkDescriptorSetLayoutCreateInfo *layout_create_info, VkDescriptorSetLayout *out);
    RendererResult DestroyDescriptorSetLayout(RendererDevice *device, VkDescriptorSetLayout *layout);

    VkDescriptorSet *m_descriptor_sets_view;
};

} // namespace hyperion

#endif