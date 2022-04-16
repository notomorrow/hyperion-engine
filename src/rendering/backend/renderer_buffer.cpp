//
// Created by emd22 on 2022-02-22.
//

#include "renderer_buffer.h"
#include "renderer_command_buffer.h"
#include "renderer_device.h"
#include "renderer_helpers.h"

#include "../../system/debug.h"

#include "renderer_features.h"

#include <vector>
#include <queue>
#include <cstring>

namespace hyperion {
namespace renderer {

StagingBuffer *StagingBufferPool::Context::Acquire(size_t required_size)
{
    if (required_size == 0) {
        DebugLog(LogType::Warn, "Attempt to acquire staging buffer of 0 size\n");

        return nullptr;
    }
    
    const size_t new_size = MathUtil::NextPowerOf2(required_size);

    StagingBuffer *staging_buffer = staging_buffer = m_pool->FindStagingBuffer(required_size - 1);

    if (staging_buffer != nullptr && m_used.find(staging_buffer) == m_used.end()) {
        DebugLog(
            LogType::Debug,
            "Requested staging buffer of size %llu, reusing existing staging buffer of size %llu\n",
            required_size,
            staging_buffer->size
        );
    } else {
        DebugLog(
            LogType::Debug,
            "Staging buffer of size >= %llu not found, creating one of size %llu at time %lld\n",
            required_size,
            new_size,
            std::time(nullptr)
        );

        staging_buffer = CreateStagingBuffer(new_size);
    }

    m_used.insert(staging_buffer);

    return staging_buffer;
}

StagingBuffer *StagingBufferPool::Context::CreateStagingBuffer(size_t size)
{
    const std::time_t current_time = std::time(nullptr);

    DebugLog(
        LogType::Debug,
        "Creating staging buffer of size %llu at time %lld\n",
        size,
        current_time
    );

    auto staging_buffer = std::make_unique<StagingBuffer>();

    if (!staging_buffer->Create(m_device, size)) {
        return nullptr;
    }

    m_staging_buffers.push_back(StagingBufferRecord{
        .size = size,
        .buffer = std::move(staging_buffer),
        .last_used = current_time
    });

    return m_staging_buffers.back().buffer.get();
}

StagingBuffer *StagingBufferPool::FindStagingBuffer(size_t size)
{
    /* do a binary search to find one with the closest size (never less than required) */
    const auto bound = std::upper_bound(m_staging_buffers.begin(), m_staging_buffers.end(), size, [](const size_t &sz, const auto &it) {
        return sz < it.size;
    });

    if (bound != m_staging_buffers.end()) {
        /* Update the time so that frequently used buffers will stay in the pool longer */
        bound->last_used = std::time(nullptr);

        return bound->buffer.get();
    }

    return nullptr;
}

Result StagingBufferPool::Use(Device *device, UseFunction &&fn)
{
    auto result = Result::OK;

    Context context(this, device);

    HYPERION_PASS_ERRORS(fn(context), result);

    for (auto &record : context.m_staging_buffers) {
        const auto bound = std::upper_bound(m_staging_buffers.begin(), m_staging_buffers.end(), record, [](const auto &record, const auto &it) {
            return record.size < it.size;
        });

        m_staging_buffers.insert(bound, std::move(record));
    }
    
    if (++use_calls % gc_threshold == 0) {
        HYPERION_PASS_ERRORS(GC(device), result);
    }

    return result;
}

Result StagingBufferPool::GC(Device *device)
{
    const auto current_time = std::time(nullptr);

    DebugLog(LogType::Debug, "Clean up staging buffers from pool\n");

    std::queue<std::vector<StagingBufferRecord>::iterator> to_remove;

    for (auto it = m_staging_buffers.begin(); it != m_staging_buffers.end(); ++it) {
        if (current_time - it->last_used > hold_time) {
            to_remove.push(it);
        }
    }

    if (to_remove.empty()) {
        DebugLog(
            LogType::Debug,
            "No staging buffers queued for removal.\n"
        );
    }

    DebugLog(LogType::Debug, "Removing %llu staging buffers from pool\n", to_remove.size());

    auto result = Result::OK;

    while (!to_remove.empty()) {
        HYPERION_PASS_ERRORS(to_remove.front()->buffer->Destroy(device), result);

        m_staging_buffers.erase(to_remove.front());

        to_remove.pop();
    }

    return result;
}

Result StagingBufferPool::Destroy(Device *device)
{
    auto result = Result::OK;

    for (auto &record : m_staging_buffers) {
        HYPERION_PASS_ERRORS(record.buffer->Destroy(device), result);
    }

    m_staging_buffers.clear();

    use_calls = 0;

    return result;
}

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
      size(0),
      map(nullptr)
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
    map = nullptr;
}

void GPUMemory::Copy(Device *device, size_t count, void *ptr)
{
    if (map == nullptr) {
        Map(device, &map);
    }

    memcpy(map, ptr, count);
}

void GPUMemory::Copy(Device *device, size_t offset, size_t count, void *ptr)
{
    if (map == nullptr) {
        Map(device, &map);
    }

    memcpy((void *)(intptr_t(map) + offset), ptr, count);
}

GPUBuffer::GPUBuffer(VkBufferUsageFlags usage_flags,
    VmaMemoryUsage vma_usage,
    VmaAllocationCreateFlags vma_allocation_create_flags
) : GPUMemory(),
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
    VkBufferMemoryBarrier staging_barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    staging_barrier.buffer = src_buffer->buffer;
    staging_barrier.size = count;
    staging_barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    staging_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    staging_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    staging_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vkCmdPipelineBarrier(
        command_buffer->GetCommandBuffer(),
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        0, nullptr,
        1, &staging_barrier,
        0, nullptr
    );

    VkBufferCopy region{};
    region.size = count;

    vkCmdCopyBuffer(
        command_buffer->GetCommandBuffer(),
        src_buffer->buffer,
        buffer,
        1,
        &region
    );
    
    VkBufferMemoryBarrier dst_barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    dst_barrier.buffer = buffer;
    dst_barrier.size = count;
    dst_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    dst_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dst_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    dst_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vkCmdPipelineBarrier(
        command_buffer->GetCommandBuffer(),
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        0, nullptr,
        1, &dst_barrier,
        0, nullptr
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
    if (map != nullptr) {
        Unmap(device);
        map = nullptr;
    }

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
    : GPUBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
{
}

StorageBuffer::StorageBuffer()
    : GPUBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
{
}

StagingBuffer::StagingBuffer()
    : GPUBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY)
{
}

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
    if (map != nullptr) {
        Unmap(device);
    }

    vmaDestroyImage(device->GetAllocator(), this->image, this->allocation);
    this->image = nullptr;
    this->allocation = nullptr;

    HYPERION_RETURN_OK;
}

} // namespace renderer
} // namespace hyperion
