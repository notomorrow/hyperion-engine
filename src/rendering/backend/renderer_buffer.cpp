//
// Created by emd22 on 2022-02-22.
//

#include "renderer_buffer.h"
#include "renderer_command_buffer.h"
#include "renderer_device.h"
#include "renderer_helpers.h"

#include "../../system/debug.h"
#include <cstring>

namespace hyperion {
namespace renderer {
uint32_t GPUMemory::FindMemoryType(Device *device, uint32_t vk_type_filter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(device->GetPhysicalDevice(), &mem_properties);

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((vk_type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            DebugLog(LogType::Info, "Found Memory type [%d]!\n", i);
            return i;
        }
    }

    AssertThrowMsg(nullptr, "Could not find suitable memory type!\n");
}

GPUMemory::GPUMemory(
    uint32_t memory_property_flags,
    uint32_t sharing_mode)
    : memory_property_flags(memory_property_flags),
      sharing_mode(sharing_mode),
      size(0)
{
    static unsigned allocations = 0;
    index = allocations++;
    DebugLog(LogType::Debug, "Allocate GPUMemory %u\n", index);
}

GPUMemory::~GPUMemory()
{
    DebugLog(LogType::Debug, "Destroy GPUMemory %u\n", index);
}


void GPUMemory::Map(Device *device, void **ptr)
{
    vmaMapMemory(device->GetAllocator(), this->allocation, ptr);
}

void GPUMemory::Unmap(Device *device)
{
    vmaUnmapMemory(device->GetAllocator(), this->allocation);
}

void GPUMemory::Copy(Device *device, size_t count, void *ptr)
{
    void *map;
    Map(device, &map);
    memcpy(map, ptr, count);
    Unmap(device);
}

void GPUMemory::Copy(Device *device, size_t offset, size_t count, void *ptr)
{
    void *map;
    Map(device, &map);
    memcpy((void *)(intptr_t(map) + offset), ptr, count);
    Unmap(device);
}

GPUBuffer::GPUBuffer(VkBufferUsageFlags usage_flags, uint32_t memory_property_flags, uint32_t sharing_mode)
    : GPUMemory(memory_property_flags, sharing_mode),
      buffer(nullptr),
      usage_flags(usage_flags)
{
}

GPUBuffer::~GPUBuffer()
{
    AssertThrowMsg(buffer == nullptr, "buffer should have been destroyed!");
}

void GPUBuffer::Create(Device *device, size_t size)
{
    if (size == 0) {
        DebugLog(LogType::Warn, "Creating empty gpu buffer\n");
    }

    this->size = size;

    const QueueFamilyIndices &qf_indices = device->GetQueueFamilyIndices();
    const uint32_t buffer_family_indices[] = { qf_indices.graphics_family.value(), qf_indices.compute_family.value() };

    VkBufferCreateInfo vk_buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    vk_buffer_info.size  = size;
    vk_buffer_info.usage = usage_flags | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vk_buffer_info.pQueueFamilyIndices = buffer_family_indices;
    vk_buffer_info.queueFamilyIndexCount = uint32_t(std::size(buffer_family_indices));

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
    alloc_info.pUserData = reinterpret_cast<void *>(index);

    auto result = vmaCreateBuffer(
        device->GetAllocator(),
        &vk_buffer_info,
        &alloc_info,
        &this->buffer,
        &this->allocation,
        nullptr
    );

    AssertThrowMsg(result == VK_SUCCESS, "Failed to create gpu buffer");
}

void GPUBuffer::Destroy(Device *device)
{
    vmaDestroyBuffer(device->GetAllocator(), this->buffer, this->allocation);
    this->buffer = nullptr;
    this->allocation = nullptr;
}


VertexBuffer::VertexBuffer(uint32_t memory_property_flags, uint32_t sharing_mode)
    : GPUBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, memory_property_flags, sharing_mode) {}

void VertexBuffer::Bind(CommandBuffer *cmd)
{
    const VkBuffer vertex_buffers[] = { buffer };
    const VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd->GetCommandBuffer(), 0, 1, vertex_buffers, offsets);
}

IndexBuffer::IndexBuffer(uint32_t memory_property_flags, uint32_t sharing_mode)
    : GPUBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, memory_property_flags, sharing_mode) {}

void IndexBuffer::Bind(CommandBuffer *cmd)
{
    vkCmdBindIndexBuffer(cmd->GetCommandBuffer(), this->buffer, 0, helpers::ToVkIndexType(GetDatumType()));
}


UniformBuffer::UniformBuffer(uint32_t memory_property_flags, uint32_t sharing_mode)
    : GPUBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, memory_property_flags, sharing_mode) {}

StorageBuffer::StorageBuffer(uint32_t memory_property_flags, uint32_t sharing_mode)
    : GPUBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, memory_property_flags, sharing_mode) {}


StagingBuffer::StagingBuffer(uint32_t memory_property_flags, uint32_t sharing_mode)
    : GPUBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, memory_property_flags, sharing_mode) {}



GPUImage::GPUImage(VkImageUsageFlags usage_flags, uint32_t memory_property_flags, uint32_t sharing_mode)
    : GPUMemory(memory_property_flags, sharing_mode),
      image(nullptr),
      usage_flags(usage_flags)
{
    std::cout << "Alloc image\n";

    if (index == 41) {
       // HYP_BREAKPOINT;
    }
}

GPUImage::~GPUImage()
{
    std::cout << "Dealloc image\n";
    AssertThrowMsg(image == nullptr, "image should have been destroyed!");
}

Result GPUImage::Create(Device *device, size_t size, VkImageCreateInfo *image_info)
{
    this->size = size;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    alloc_info.pUserData = reinterpret_cast<void *>(index);

    HYPERION_VK_CHECK(vmaCreateImage(device->GetAllocator(), image_info, &alloc_info, &this->image, &this->allocation, nullptr));

    HYPERION_RETURN_OK;
}

Result GPUImage::Destroy(Device *device)
{
    vmaDestroyImage(device->GetAllocator(), this->image, this->allocation);
    this->image = nullptr;
    this->allocation = nullptr;

    HYPERION_RETURN_OK;
}

} // namespace renderer
} // namespace hyperion
