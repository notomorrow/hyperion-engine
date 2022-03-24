//
// Created by emd22 on 2022-02-22.
//

#include "renderer_buffer.h"
#include "renderer_device.h"

#include "../../system/debug.h"
#include <cstring>

namespace hyperion {
namespace renderer {
uint32_t GPUMemory::FindMemoryType(Device *device, uint32_t vk_type_filter, VkMemoryPropertyFlags properties) {
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
      size(0),
      memory(nullptr)
{
    static unsigned allocations = 0;
    DebugLog(LogType::Debug, "Allocate GPUMemory %d\n", allocations++);
}

GPUMemory::~GPUMemory()
{
    static unsigned destructions = 0;
    DebugLog(LogType::Debug, "Destroy GPUMemory %d\n", destructions++);
    AssertThrowMsg(memory == nullptr, "memory should have been destroyed!");
}


void GPUMemory::Map(Device *device, void **ptr) {
    vmaMapMemory(*device->GetAllocator(), this->allocation, ptr);
}

void GPUMemory::Unmap(Device *device) {
    vmaUnmapMemory(*device->GetAllocator(), this->allocation);
}

void GPUMemory::Copy(Device *device, size_t size, void *ptr) {
    void *map;
    this->Map(device, &map);
    memcpy(map, ptr, size);
    this->Unmap(device);
}

GPUBuffer::GPUBuffer(VkBufferUsageFlags usage_flags, uint32_t memory_property_flags, uint32_t sharing_mode)
    : GPUMemory(memory_property_flags, sharing_mode),
      usage_flags(usage_flags),
      buffer(nullptr)
{
}

GPUBuffer::~GPUBuffer()
{
    AssertThrowMsg(buffer == nullptr, "buffer should have been destroyed!");
}

void GPUBuffer::Create(Device *device, size_t size) {
    VkBufferCreateInfo vk_buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    this->size = size;

    vk_buffer_info.size  = size;
    vk_buffer_info.usage = (VkBufferUsageFlags)this->usage_flags | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    alloc_info.usage = VMA_MEMORY_USAGE_AUTO;

    vmaCreateBuffer(*device->GetAllocator(), &vk_buffer_info, &alloc_info, &this->buffer, &this->allocation, nullptr);
}

void GPUBuffer::Destroy(Device *device)
{
    VkDevice vk_device = device->GetDevice();
    vmaDestroyBuffer(*device->GetAllocator(), this->buffer, this->allocation);
    this->buffer = nullptr;
    this->allocation = nullptr;
}


VertexBuffer::VertexBuffer(uint32_t memory_property_flags, uint32_t sharing_mode)
    : GPUBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, memory_property_flags, sharing_mode) {}

void VertexBuffer::BindBuffer(VkCommandBuffer *cmd) {
    const VkBuffer vertex_buffers[] = { buffer };
    const VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(*cmd, 0, 1, vertex_buffers, offsets);
}

UniformBuffer::UniformBuffer(uint32_t memory_property_flags, uint32_t sharing_mode)
    : GPUBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, memory_property_flags, sharing_mode) {}

StorageBuffer::StorageBuffer(uint32_t memory_property_flags, uint32_t sharing_mode)
    : GPUBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, memory_property_flags, sharing_mode) {}


StagingBuffer::StagingBuffer(uint32_t memory_property_flags, uint32_t sharing_mode)
    : GPUBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, memory_property_flags, sharing_mode) {}



GPUImage::GPUImage(VkImageUsageFlags usage_flags, uint32_t memory_property_flags, uint32_t sharing_mode)
    : GPUMemory(memory_property_flags, sharing_mode),
      usage_flags(usage_flags),
      image(nullptr)
{
}

GPUImage::~GPUImage()
{
    AssertThrowMsg(image == nullptr, "image should have been destroyed!");
}

Result GPUImage::Create(Device *device, size_t size, VkImageCreateInfo *image_info)
{
    this->size = size;

    VkDevice vk_device = device->GetDevice();

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    auto result = vmaCreateImage(*device->GetAllocator(), image_info, &alloc_info, &this->image, &this->allocation, nullptr);

    HYPERION_RETURN_OK;
}

Result GPUImage::Destroy(Device *device)
{
    VkDevice vk_device = device->GetDevice();

    vmaDestroyImage(*device->GetAllocator(), this->image, this->allocation);
    this->image = nullptr;
    this->allocation = nullptr;

    HYPERION_RETURN_OK;
}

} // namespace renderer
} // namespace hyperion
