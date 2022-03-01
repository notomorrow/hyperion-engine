#include "uniform_buffer.h"
#include "shader.h"
#include "../gl_util.h"
#include "../opengl.h"

namespace hyperion {

UniformBufferInternalsHolder::UniformBufferInternalsHolder() {}
UniformBufferInternalsHolder::~UniformBufferInternalsHolder()
{
    Reset();
}

UniformBuffer::Internal *UniformBufferInternalsHolder::CreateUniformBufferInternal(RendererDevice *device, Shader *shader, UniformBuffer &uniform_buffer)
{
    size_t total_size = 0; // calculate total size of data allocated for uniform buffer
    for (size_t i = 0; i < uniform_buffer.data.size(); i++) {
        total_size += uniform_buffer.data[i].value.GetSize();
    }

    unsigned char *raw_uniform_data = new unsigned char[total_size]; // TODO: more linear layout for the above data table to ease copying
    // OR add an offset parameter to Copy() to facilitate doing this in a loop
    size_t offset = 0;
    for (size_t i = 0; i < uniform_buffer.data.size(); i++) {
        size_t sz = uniform_buffer.data[i].value.GetSize();
        std::memcpy(raw_uniform_data + offset, uniform_buffer.data[i].value.GetRawPtr(), sz);
        offset += sz;
    }

    UniformBuffer::Internal *_internal = new UniformBuffer::Internal;
    _internal->generated = false;

    _internal->gpu_buffer = new RendererGPUBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    _internal->gpu_buffer->Create(device, total_size);
    _internal->gpu_buffer->Copy(device, total_size, raw_uniform_data);

    delete[] raw_uniform_data;

    // TODO: rest

   /* VkBufferCreateInfo buf_info = {};
    buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_info.pNext = NULL;
    buf_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    buf_info.size = total_size;
    buf_info.queueFamilyIndexCount = 0;
    buf_info.pQueueFamilyIndices = NULL;
    buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buf_info.flags = 0;
    auto res = vkCreateBuffer(*device, &buf_info, NULL, &_internal->handle);
    AssertThrow(res == VK_SUCCESS);




    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(*device, _internal->handle,
        &mem_reqs);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.pNext = NULL;
    alloc_info.memoryTypeIndex = 0;

    alloc_info.allocationSize = mem_reqs.size;
    pass = memory_type_from_properties(info, mem_reqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &alloc_info.memoryTypeIndex);

    res = vkAllocateMemory(info.device, &alloc_info, NULL,
        &(info.uniform_data.mem));*/

    m_internals.push_back(_internal);

    return _internal;
}

void UniformBufferInternalsHolder::DestroyUniformBufferInternal(UniformBuffer::Internal *_internal)
{
    auto it = std::find(
        m_internals.begin(),
        m_internals.end(),
        _internal
    );

    AssertThrow(it != m_internals.end());

    if (_internal->generated) {
        // TODO:
    }

    delete _internal;

    m_internals.erase(it);
}

void UniformBufferInternalsHolder::Reset()
{
    for (auto *uniform_buffer : m_internals) {
        DestroyUniformBufferInternal(uniform_buffer);
    }

    m_internals.clear();
}

} // namespace hyperion