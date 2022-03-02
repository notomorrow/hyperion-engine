//
// Created by emd22 on 2022-02-22.
//

#ifndef HYPERION_RENDERER_BUFFER_H
#define HYPERION_RENDERER_BUFFER_H

#include "renderer_device.h"
#include "renderer_result.h"

namespace hyperion {

class RendererGPUMemory {
public:
    static uint32_t FindMemoryType(RendererDevice *device, uint32_t vk_type_filter, VkMemoryPropertyFlags properties);
    RendererGPUMemory(
        uint32_t memory_property_flags,
        uint32_t sharing_mode
    );

    VkDeviceMemory memory;
    VkDeviceSize size;

    void Map(RendererDevice *device, void **ptr);
    void Unmap(RendererDevice *device);
    void Copy(RendererDevice *device, size_t size, void *ptr);

protected:
    uint32_t sharing_mode;
    uint32_t memory_property_flags;
};

/* buffers */

class RendererGPUBuffer : public RendererGPUMemory {
public:
    RendererGPUBuffer(
        VkBufferUsageFlags usage_flags,
        uint32_t memory_property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        uint32_t sharing_mode = VK_SHARING_MODE_EXCLUSIVE
    );
    RendererGPUBuffer(const RendererGPUBuffer &other) = delete;
    RendererGPUBuffer &operator=(const RendererGPUBuffer &other) = delete;
    ~RendererGPUBuffer();

    void Create(RendererDevice *device, size_t buffer_size);
    void Destroy(RendererDevice *device);

    VkBuffer buffer;

private:
    VkBufferUsageFlags usage_flags;
};

class RendererVertexBuffer : public RendererGPUBuffer {
public:
    RendererVertexBuffer(
        uint32_t memory_property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        uint32_t sharing_mode = VK_SHARING_MODE_EXCLUSIVE
    );

    void BindBuffer(VkCommandBuffer *command_buffer);
};

class RendererUniformBuffer : public RendererGPUBuffer {
public:
    RendererUniformBuffer(
        uint32_t memory_property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        uint32_t sharing_mode = VK_SHARING_MODE_EXCLUSIVE
    );
};

class RendererStagingBuffer : public RendererGPUBuffer {
public:
    RendererStagingBuffer(
        uint32_t memory_property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        uint32_t sharing_mode = VK_SHARING_MODE_EXCLUSIVE
    );
};

/* images */


class RendererGPUImage : public RendererGPUMemory {
public:
    RendererGPUImage(
        VkImageUsageFlags usage_flags,
        uint32_t memory_property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        uint32_t sharing_mode = VK_SHARING_MODE_EXCLUSIVE
    );
    RendererGPUImage(const RendererGPUImage &other) = delete;
    RendererGPUImage &operator=(const RendererGPUImage &other) = delete;
    ~RendererGPUImage();

    RendererResult Create(RendererDevice *device, size_t size, VkImageCreateInfo *image_info);
    RendererResult Destroy(RendererDevice *device);

    VkImage image;

private:
    VkImageUsageFlags usage_flags;
};
}; // namespace hyperion

#endif //HYPERION_RENDERER_BUFFER_H
