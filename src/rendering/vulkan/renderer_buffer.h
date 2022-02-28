//
// Created by emd22 on 2022-02-22.
//

#ifndef HYPERION_RENDERER_BUFFER_H
#define HYPERION_RENDERER_BUFFER_H

#include "renderer_device.h"

namespace hyperion {

class RendererGPUBuffer {
public:
    RendererGPUBuffer();

    void SetMemoryPropertyFlags(uint32_t flags);
    void SetSharingMode(VkSharingMode sharing_mode);

    static uint32_t FindMemoryType(RendererDevice *device, uint32_t vk_type_filter, VkMemoryPropertyFlags properties);
    void Create(RendererDevice *device, VkBufferUsageFlags usage_flags, VkDeviceSize buffer_size);

    void Map(RendererDevice *device, void **ptr);
    void Unmap(RendererDevice *device);

    VkBuffer buffer;
    VkDeviceMemory memory;

    VkDeviceSize size;
private:
    VkSharingMode sharing_mode;
    uint32_t memory_property_flags;
};

class RendererVertexBuffer {
public:
    void Create(RendererDevice *device, VkDeviceSize buffer_size);
    void Copy(RendererDevice *device, size_t size, void *ptr);
    void BindBuffer(VkCommandBuffer *command_buffer);


    VkBuffer GetVkBuffer() {
        return this->gpu_buffer->buffer;
    }

    RendererGPUBuffer *gpu_buffer;
};

}; /* namespace hyperion */

#endif //HYPERION_RENDERER_BUFFER_H
