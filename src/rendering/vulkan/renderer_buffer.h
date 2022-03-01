//
// Created by emd22 on 2022-02-22.
//

#ifndef HYPERION_RENDERER_BUFFER_H
#define HYPERION_RENDERER_BUFFER_H

#include "renderer_device.h"

namespace hyperion {

class RendererGPUBuffer {
public:
    RendererGPUBuffer(
        uint32_t usage_flags,
        uint32_t memory_property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        uint32_t sharing_mode = VK_SHARING_MODE_EXCLUSIVE
    );

    static uint32_t FindMemoryType(RendererDevice *device, uint32_t vk_type_filter, VkMemoryPropertyFlags properties);
    void Create(RendererDevice *device, VkDeviceSize buffer_size);
    void Copy(RendererDevice *device, size_t size, void *ptr);
    void Destroy(RendererDevice *device);

    void Map(RendererDevice *device, void **ptr);
    void Unmap(RendererDevice *device);

    VkBuffer buffer;
    VkDeviceMemory memory;

    VkDeviceSize size;

private:
    uint32_t sharing_mode;
    uint32_t memory_property_flags;
    uint32_t usage_flags;
};

class RendererVertexBuffer : public RendererGPUBuffer {
public:
    RendererVertexBuffer(
        uint32_t memory_property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        uint32_t sharing_mode = VK_SHARING_MODE_EXCLUSIVE
    );

    void BindBuffer(VkCommandBuffer *command_buffer);
};

}; /* namespace hyperion */

#endif //HYPERION_RENDERER_BUFFER_H
