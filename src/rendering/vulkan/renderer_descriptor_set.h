#ifndef HYPERION_RENDERER_DESCRIPTOR_SET_H
#define HYPERION_RENDERER_DESCRIPTOR_SET_H

#include "renderer_descriptor.h"
#include "renderer_descriptor_pool.h"
#include "renderer_result.h"

#include <vector>
#include <memory>

namespace hyperion {
class RendererDevice;
class RendererDescriptorPool;
class RendererDescriptorSet {
public:
    RendererDescriptorSet();
    RendererDescriptorSet(const RendererDescriptorSet &other) = delete;
    RendererDescriptorSet &operator=(const RendererDescriptorSet &other) = delete;
    ~RendererDescriptorSet();

    RendererDescriptor *AddDescriptor(uint32_t binding,
        size_t size,
        VkDescriptorType type,
        VkBufferUsageFlags usage_flags,
        VkShaderStageFlags stage_flags);

    inline RendererDescriptor *GetDescriptor(size_t index) { return m_descriptors[index].get(); }
    inline const RendererDescriptor *GetDescriptor(size_t index) const { return m_descriptors[index].get(); }

    RendererResult Create(RendererDevice *device, RendererDescriptorPool *pool);
    RendererResult Destroy(RendererDevice *device);

    VkDescriptorSet m_set;

private:
    std::vector<std::unique_ptr<RendererDescriptor>> m_descriptors;

};

} // namespace hyperion

#endif