//
// Created by emd22 on 2022-02-22.
//

#include "renderer_buffer.h"

#include "../../system/debug.h"
#include <cstring>

namespace hyperion {

uint32_t RendererGPUBuffer::FindMemoryType(RendererDevice *device, uint32_t vk_type_filter, VkMemoryPropertyFlags properties) {
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

RendererGPUBuffer::RendererGPUBuffer() {
    this->SetMemoryPropertyFlags((VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
    this->SetSharingMode(VK_SHARING_MODE_EXCLUSIVE);
    this->size = 0;
}

void RendererGPUBuffer::SetMemoryPropertyFlags(uint32_t flags) {
    this->memory_property_flags = flags;
}
void RendererGPUBuffer::SetSharingMode(VkSharingMode sharing_mode) {
    this->sharing_mode = sharing_mode;
}

void RendererGPUBuffer::Create(RendererDevice *device, VkBufferUsageFlags usage_flags, VkDeviceSize buffer_size) {
    VkDevice vk_device = device->GetDevice();

    VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_info.size = buffer_size;
    buffer_info.usage = usage_flags;
    buffer_info.sharingMode = this->sharing_mode;

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

void RendererGPUBuffer::Map(RendererDevice *device, void **ptr) {
    vkMapMemory(device->GetDevice(), this->memory, 0, this->size, 0, ptr);
}
void RendererGPUBuffer::Unmap(RendererDevice *device) {
    vkUnmapMemory(device->GetDevice(), this->memory);
}

void RendererVertexBuffer::Copy(RendererDevice *device, size_t size, void *ptr) {
    void *map;
    this->gpu_buffer->Map(device, &map);
    memcpy(map, ptr, size);
    this->gpu_buffer->Unmap(device);
}

void RendererVertexBuffer::BindBuffer(VkCommandBuffer *cmd) {
    const VkBuffer vertex_buffers[] = { this->GetVkBuffer() };
    const VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(*cmd, 0, 1, vertex_buffers, offsets);
}

void RendererVertexBuffer::Create(RendererDevice *device, VkDeviceSize buffer_size) {
    auto *buffer = new RendererGPUBuffer();
    buffer->SetMemoryPropertyFlags((VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
    buffer->SetSharingMode(VK_SHARING_MODE_EXCLUSIVE);
    buffer->Create(device, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, buffer_size);

    this->gpu_buffer = buffer;
}

}; /* namespace hyperion */
