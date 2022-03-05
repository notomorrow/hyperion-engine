//
// Created by emd22 on 2022-02-22.
//

#include "renderer_buffer.h"

#include "../../system/debug.h"
#include <cstring>

namespace hyperion {

uint32_t RendererGPUMemory::FindMemoryType(RendererDevice *device, uint32_t vk_type_filter, VkMemoryPropertyFlags properties) {
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

RendererGPUMemory::RendererGPUMemory(
    uint32_t memory_property_flags,
    uint32_t sharing_mode)
    : memory_property_flags(memory_property_flags),
      sharing_mode(sharing_mode),
      size(0),
      memory(nullptr)
{
}

void RendererGPUMemory::Map(RendererDevice *device, void **ptr) {
    vkMapMemory(device->GetDevice(), this->memory, 0, this->size, 0, ptr);
}

void RendererGPUMemory::Unmap(RendererDevice *device) {
    vkUnmapMemory(device->GetDevice(), this->memory);
}

void RendererGPUMemory::Copy(RendererDevice *device, size_t size, void *ptr) {
    void *map;
    Map(device, &map);
    memcpy(map, ptr, size);
    Unmap(device);
}

RendererGPUBuffer::RendererGPUBuffer(VkBufferUsageFlags usage_flags, uint32_t memory_property_flags, uint32_t sharing_mode)
    : RendererGPUMemory(memory_property_flags, sharing_mode),
      usage_flags(usage_flags),
      buffer(nullptr)
{
}

RendererGPUBuffer::~RendererGPUBuffer()
{
    AssertExitMsg(memory == nullptr, "memory should have been destroyed!");
    AssertExitMsg(buffer == nullptr, "buffer should have been destroyed!");
}

void RendererGPUBuffer::Create(RendererDevice *device, size_t size) {
    VkDevice vk_device = device->GetDevice();

    VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_info.size = size;
    buffer_info.usage = (VkBufferUsageFlags)usage_flags;
    buffer_info.sharingMode = (VkSharingMode)this->sharing_mode;


    QueueFamilyIndices family_indices = device->FindQueueFamilies();

    uint32_t indices_array[] = { family_indices.graphics_family.value(), family_indices.transfer_family.value() };
    buffer_info.pQueueFamilyIndices = indices_array;
    buffer_info.queueFamilyIndexCount = sizeof(indices_array) / sizeof(indices_array[0]);

    auto result = vkCreateBuffer(vk_device, &buffer_info, nullptr, &this->buffer);
    AssertThrowMsg(result == VK_SUCCESS, "Could not create vulkan vertex buffer!\n");

    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(vk_device, this->buffer, &requirements);

    VkMemoryAllocateInfo alloc_info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc_info.allocationSize = requirements.size;
    //this->memory_property_flags = (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    alloc_info.memoryTypeIndex = FindMemoryType(device, requirements.memoryTypeBits, this->memory_property_flags);

    result = vkAllocateMemory(vk_device, &alloc_info, nullptr, &this->memory);
    AssertThrowMsg(result == VK_SUCCESS, "Could not allocate video memory!\n");

    vkBindBufferMemory(vk_device, this->buffer, this->memory, 0);
    this->size = buffer_info.size;
}

void RendererGPUBuffer::Destroy(RendererDevice *device)
{
    VkDevice vk_device = device->GetDevice();

    vkFreeMemory(vk_device, memory, nullptr);
    memory = nullptr;

    vkDestroyBuffer(vk_device, buffer, nullptr);
    buffer = nullptr;
}


RendererVertexBuffer::RendererVertexBuffer(uint32_t memory_property_flags, uint32_t sharing_mode)
    : RendererGPUBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, memory_property_flags, sharing_mode) {}

void RendererVertexBuffer::BindBuffer(VkCommandBuffer *cmd) {
    const VkBuffer vertex_buffers[] = { buffer };
    const VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(*cmd, 0, 1, vertex_buffers, offsets);
}

RendererUniformBuffer::RendererUniformBuffer(uint32_t memory_property_flags, uint32_t sharing_mode)
    : RendererGPUBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, memory_property_flags, sharing_mode) {}


RendererStagingBuffer::RendererStagingBuffer(uint32_t memory_property_flags, uint32_t sharing_mode)
    : RendererGPUBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, memory_property_flags, sharing_mode) {}



RendererGPUImage::RendererGPUImage(VkImageUsageFlags usage_flags, uint32_t memory_property_flags, uint32_t sharing_mode)
    : RendererGPUMemory(memory_property_flags, sharing_mode),
      usage_flags(usage_flags),
      image(nullptr)
{
}

RendererGPUImage::~RendererGPUImage()
{
    AssertExit(memory == nullptr);
    AssertExit(image == nullptr);
}

RendererResult RendererGPUImage::Create(RendererDevice *device, size_t size, VkImageCreateInfo *image_info) {
    this->size = size;

    VkDevice vk_device = device->GetDevice();

    HYPERION_VK_CHECK_MSG(vkCreateImage(device->GetDevice(), image_info, nullptr, &image), "Could not create image!");

    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(vk_device, image, &requirements);

    VkMemoryAllocateInfo alloc_info{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    alloc_info.allocationSize = requirements.size;
    //this->memory_property_flags = (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    alloc_info.memoryTypeIndex = FindMemoryType(device, requirements.memoryTypeBits, this->memory_property_flags);

    HYPERION_VK_CHECK_MSG(vkAllocateMemory(vk_device, &alloc_info, nullptr, &this->memory), "Could not allocate image memory!");
    HYPERION_VK_CHECK_MSG(vkBindImageMemory(vk_device, this->image, this->memory, 0), "Could not bind image memory!");

    return RendererResult(RendererResult::RENDERER_OK);
}

RendererResult RendererGPUImage::Destroy(RendererDevice *device)
{
    VkDevice vk_device = device->GetDevice();

    vkFreeMemory(vk_device, memory, nullptr);
    memory = nullptr;

    vkDestroyImage(vk_device, image, nullptr);
    image = nullptr;

    return RendererResult(RendererResult::RENDERER_OK);
}

}; /* namespace hyperion */
