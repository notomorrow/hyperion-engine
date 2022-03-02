#ifndef HYPERION_RENDERER_DESCRIPTOR_H
#define HYPERION_RENDERER_DESCRIPTOR_H

#include "renderer_buffer.h"
#include "../uniform_buffer.h"

#include <vector>

namespace hyperion {

class RendererDevice;

class RendererDescriptor {
    friend class RendererDescriptorSet;
public:
    RendererDescriptor(uint32_t binding, size_t size, VkDescriptorType type, VkBufferUsageFlags usage_flags, VkShaderStageFlags stage_flags);
    RendererDescriptor(const RendererDescriptor &other) = delete;
    RendererDescriptor &operator=(const RendererDescriptor &other) = delete;
    ~RendererDescriptor();

    inline RendererGPUBuffer *GetBuffer() { return m_buffer; }
    inline const RendererGPUBuffer *GetBuffer() const { return m_buffer; }

    void Create(RendererDevice *device);
    void Destroy(RendererDevice *device);

protected:
    RendererGPUBuffer *m_buffer;

    VkDescriptorBufferInfo m_buffer_info;
    VkDescriptorImageInfo m_image_info;
    
    uint32_t m_binding;
    size_t m_size;
    VkDescriptorType m_type;
    VkBufferUsageFlags m_usage_flags;
    VkShaderStageFlags m_stage_flags;
};

class RendererUniformBufferDescriptor : public RendererDescriptor {
public:
    RendererUniformBufferDescriptor(uint32_t binding, size_t size, VkDescriptorType type, VkShaderStageFlags stage_flags);
    RendererUniformBufferDescriptor(const RendererUniformBufferDescriptor &other) = delete;
    RendererUniformBufferDescriptor &operator=(const RendererUniformBufferDescriptor &other) = delete;
    ~RendererUniformBufferDescriptor() = default;

    void Create(RendererDevice *device, const UniformBuffer &uniform_buffer);
};

} // namespace hyperion

#endif