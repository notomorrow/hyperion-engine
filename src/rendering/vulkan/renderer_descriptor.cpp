#include "renderer_descriptor_set.h"
#include "renderer_device.h"
#include "../../system/debug.h"

namespace hyperion {
RendererDescriptor::RendererDescriptor(uint32_t binding, size_t size, VkDescriptorType type, VkBufferUsageFlags usage_flags, VkShaderStageFlags stage_flags)
    : m_buffer(nullptr),
      m_binding(binding),
      m_size(size),
      m_type(type),
      m_usage_flags(usage_flags),
      m_stage_flags(stage_flags)
{
}

RendererDescriptor::~RendererDescriptor()
{
    AssertExitMsg(m_buffer == nullptr, "buffer should have been destroyed");
}

void RendererDescriptor::Create(RendererDevice *device)
{
    AssertExit(m_buffer == nullptr);

    m_buffer = new RendererGPUBuffer(m_usage_flags);
    m_buffer->Create(device, m_size);

    m_buffer_info.buffer = m_buffer->buffer;
    m_buffer_info.offset = 0; // TODO: re-use of buffers and offsetting of data
    m_buffer_info.range = m_size;
    // TODO: override class for image info
}

void RendererDescriptor::Destroy(RendererDevice *device)
{
    AssertExit(m_buffer != nullptr);

    m_buffer->Destroy(device);

    delete m_buffer;
    m_buffer = nullptr;
}

RendererUniformBufferDescriptor::RendererUniformBufferDescriptor(uint32_t binding, size_t size, VkDescriptorType type, VkShaderStageFlags stage_flags)
    : RendererDescriptor(binding, size, type, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, stage_flags) {}

void RendererUniformBufferDescriptor::Create(RendererDevice *device, const UniformBuffer &uniform_buffer)
{
    AssertExit(m_buffer == nullptr);

    m_buffer = new RendererUniformBuffer();
    m_buffer->Create(device, uniform_buffer.TotalSize());

    m_buffer_info.buffer = m_buffer->buffer;
    m_buffer_info.offset = 0; // TODO: re-use of buffers and offsetting of data
    m_buffer_info.range = m_size;
    // TODO: override class for image info
}

} // namespace hyperion