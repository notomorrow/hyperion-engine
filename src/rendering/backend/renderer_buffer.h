//
// Created by emd22 on 2022-02-22.
//

#ifndef HYPERION_RENDERER_BUFFER_H
#define HYPERION_RENDERER_BUFFER_H

#include "renderer_result.h"
#include "renderer_structs.h"
#include "../../system/vma/vma_usage.h"

namespace hyperion {
namespace renderer {

class Device;
class CommandBuffer;

class GPUMemory {
public:
    static uint32_t FindMemoryType(Device *device, uint32_t vk_type_filter, VkMemoryPropertyFlags properties);

    GPUMemory();
    GPUMemory(const GPUMemory &other) = delete;
    GPUMemory &operator=(const GPUMemory &other) = delete;
    ~GPUMemory();
    
    VmaAllocation allocation;
    VkDeviceSize size;

    void Map(Device *device, void **ptr);
    void Unmap(Device *device);
    void Copy(Device *device, size_t count, void *ptr);
    void Copy(Device *device, size_t offset, size_t count, void *ptr);

protected:
    uint32_t sharing_mode;

    uint32_t index;
};

/* buffers */

class GPUBuffer : public GPUMemory {
public:
    GPUBuffer(
        VkBufferUsageFlags usage_flags,
        VmaMemoryUsage vma_usage = VMA_MEMORY_USAGE_AUTO,
        VmaAllocationCreateFlags vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
    );
    GPUBuffer(const GPUBuffer &other) = delete;
    GPUBuffer &operator=(const GPUBuffer &other) = delete;
    ~GPUBuffer();

    void CopyFrom(CommandBuffer *command_buffer, const GPUBuffer *src_buffer, size_t count);

    Result CheckCanAllocate(Device *device, size_t size) const;

    [[nodiscard]] Result Create(Device *device, size_t buffer_size);
    [[nodiscard]] Result Destroy(Device *device);

    VkBuffer buffer;

private:
    Result CheckCanAllocate(Device *device,
        const VkBufferCreateInfo &buffer_create_info,
        const VmaAllocationCreateInfo &allocation_create_info,
        size_t size) const;

    VmaAllocationCreateInfo GetAllocationCreateInfo(Device *device) const;
    VkBufferCreateInfo GetBufferCreateInfo(Device *device) const;

    VkBufferUsageFlags       usage_flags;
    VmaMemoryUsage           vma_usage;
    VmaAllocationCreateFlags vma_allocation_create_flags;
};

class VertexBuffer : public GPUBuffer {
public:
    VertexBuffer();

    void Bind(CommandBuffer *command_buffer);
};

class IndexBuffer : public GPUBuffer {
public:
    IndexBuffer();

    void Bind(CommandBuffer *command_buffer);

    inline DatumType GetDatumType() const { return m_datum_type; }
    inline void SetDatumType(DatumType datum_type) { m_datum_type = datum_type; }

private:
    DatumType m_datum_type = DatumType::UNSIGNED_INT;
};

class UniformBuffer : public GPUBuffer {
public:
    UniformBuffer();
};

class StorageBuffer : public GPUBuffer {
public:
    StorageBuffer();
};

class StagingBuffer : public GPUBuffer {
public:
    StagingBuffer();
};

/* images */


class GPUImage : public GPUMemory {
public:
    GPUImage(VkImageUsageFlags usage_flags);
    GPUImage(const GPUImage &other) = delete;
    GPUImage &operator=(const GPUImage &other) = delete;
    ~GPUImage();

    Result Create(Device *device, size_t size, VkImageCreateInfo *image_info);
    Result Destroy(Device *device);

    VkImage image;

private:
    VkImageUsageFlags usage_flags;
};

} // namespace renderer
} // namespace hyperion

#endif //HYPERION_RENDERER_BUFFER_H
