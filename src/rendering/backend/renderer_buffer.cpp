//
// Created by emd22 on 2022-02-22.
//

#include "renderer_buffer.h"
#include "renderer_command_buffer.h"
#include "renderer_device.h"
#include "renderer_helpers.h"

#include "../../system/debug.h"
#include <cstring>

#include "renderer_features.h"

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

GPUMemory::GPUMemory()
    : sharing_mode(VK_SHARING_MODE_EXCLUSIVE),
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

GPUBuffer::GPUBuffer(VkBufferUsageFlags usage_flags,
    VmaMemoryUsage vma_usage,
    VmaAllocationCreateFlags vma_allocation_create_flags
)
    : GPUMemory(),
      buffer(nullptr),
      usage_flags(usage_flags),
      vma_usage(vma_usage),
      vma_allocation_create_flags(vma_allocation_create_flags)
{
}

GPUBuffer::~GPUBuffer()
{
    AssertThrowMsg(buffer == nullptr, "buffer should have been destroyed!");
}

VkBufferCreateInfo GPUBuffer::GetBufferCreateInfo(Device *device) const
{
    const QueueFamilyIndices &qf_indices = device->GetQueueFamilyIndices();
    const uint32_t buffer_family_indices[] = { qf_indices.graphics_family.value(), qf_indices.compute_family.value() };

    VkBufferCreateInfo vk_buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    vk_buffer_info.size  = size;
    vk_buffer_info.usage = usage_flags | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vk_buffer_info.pQueueFamilyIndices = buffer_family_indices;
    vk_buffer_info.queueFamilyIndexCount = uint32_t(std::size(buffer_family_indices));

    return vk_buffer_info;
}

VmaAllocationCreateInfo GPUBuffer::GetAllocationCreateInfo(Device *device) const
{
    VmaAllocationCreateInfo alloc_info{};
    alloc_info.flags = vma_allocation_create_flags;
    alloc_info.usage = vma_usage;
    alloc_info.pUserData = reinterpret_cast<void *>(index);

    return alloc_info;
}

Result GPUBuffer::CheckCanAllocate(Device *device, size_t size) const
{
    const auto create_info = GetBufferCreateInfo(device);
    const auto alloc_info = GetAllocationCreateInfo(device);

    return CheckCanAllocate(device, create_info, alloc_info, this->size);
}

Result GPUBuffer::CheckCanAllocate(Device *device,
    const VkBufferCreateInfo &buffer_create_info,
    const VmaAllocationCreateInfo &allocation_create_info,
    size_t size) const
{
    const Features &features = device->GetFeatures();

    auto result = Result::OK;

    uint32_t memory_type_index = UINT32_MAX;

    HYPERION_VK_PASS_ERRORS(
        vmaFindMemoryTypeIndexForBufferInfo(
            device->GetAllocator(),
            &buffer_create_info,
            &allocation_create_info,
            &memory_type_index
        ),
        result
    );

    /* check that we have enough space in the memory type */
    const auto &memory_properties = features.GetPhysicalDeviceMemoryProperties();

    AssertThrow(memory_type_index < memory_properties.memoryTypeCount);

    const auto heap_index = memory_properties.memoryTypes[memory_type_index].heapIndex;
    const auto &heap = memory_properties.memoryHeaps[heap_index];

    if (heap.size < size) {
        return {Result::RENDERER_ERR, "Heap size is less than requested size. "
            "Maybe the wrong memory type is being used, or the device is simply out of memory."};
    }

    return result;
}

void GPUBuffer::CopyFrom(CommandBuffer *command_buffer, const GPUBuffer *src_buffer, size_t count)
{
    VkBufferCopy region{};
    region.size = count;

    vkCmdCopyBuffer(
        command_buffer->GetCommandBuffer(),
        src_buffer->buffer,
        buffer,
        1,
        &region
    );
}

Result GPUBuffer::Create(Device *device, size_t size)
{
    this->size = size;

    if (size == 0) {
        return {Result::RENDERER_ERR, "Creating empty gpu buffer will result in errors \n"};
    }

    DebugLog(
        LogType::Debug,
        "Allocating GPU buffer with flags:\t(buffer usage: %lu, alloc create: %lu, alloc usage: %lu)\n",
        usage_flags,
        vma_allocation_create_flags,
        vma_usage
    );

    const auto create_info = GetBufferCreateInfo(device);
    const auto alloc_info = GetAllocationCreateInfo(device);

    HYPERION_BUBBLE_ERRORS(CheckCanAllocate(device, create_info, alloc_info, this->size));

    HYPERION_VK_CHECK_MSG(vmaCreateBuffer(
        device->GetAllocator(),
        &create_info,
        &alloc_info,
        &this->buffer,
        &this->allocation,
        nullptr
    ), "Failed to create gpu buffer");

    HYPERION_RETURN_OK;
}

Result GPUBuffer::Destroy(Device *device)
{
    vmaDestroyBuffer(device->GetAllocator(), this->buffer, this->allocation);
    this->buffer = nullptr;
    this->allocation = nullptr;

    HYPERION_RETURN_OK;
}


VertexBuffer::VertexBuffer()
    : GPUBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY)
{
}

void VertexBuffer::Bind(CommandBuffer *cmd)
{
    const VkBuffer vertex_buffers[] = { buffer };
    const VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd->GetCommandBuffer(), 0, 1, vertex_buffers, offsets);
}

IndexBuffer::IndexBuffer()
    : GPUBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY)
{
}

void IndexBuffer::Bind(CommandBuffer *cmd)
{
    vkCmdBindIndexBuffer(cmd->GetCommandBuffer(), this->buffer, 0, helpers::ToVkIndexType(GetDatumType()));
}


UniformBuffer::UniformBuffer()
    : GPUBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) {}

StorageBuffer::StorageBuffer()
    : GPUBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) {}


StagingBuffer::StagingBuffer()
    : GPUBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT) {}



GPUImage::GPUImage(VkImageUsageFlags usage_flags)
    : GPUMemory(),
      image(nullptr),
      usage_flags(usage_flags)
{
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
