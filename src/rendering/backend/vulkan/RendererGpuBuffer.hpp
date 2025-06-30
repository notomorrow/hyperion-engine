/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_BUFFER_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_BUFFER_HPP

#include <rendering/backend/RendererGpuBuffer.hpp>

#include <system/vma/VmaUsage.hpp>

namespace hyperion {
class VulkanGpuBuffer final : public GpuBufferBase
{
public:
    HYP_API VulkanGpuBuffer(GpuBufferType type, SizeType size, SizeType alignment = 0);
    HYP_API virtual ~VulkanGpuBuffer() override;

    HYP_FORCE_INLINE VkBuffer GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_API virtual bool IsCreated() const override;
    HYP_API virtual bool IsCpuAccessible() const override;

    HYP_API virtual void InsertBarrier(CommandBufferBase* commandBuffer, ResourceState newState) const override;
    HYP_API virtual void InsertBarrier(CommandBufferBase* commandBuffer, ResourceState newState, ShaderModuleType shaderType) const override;

    HYP_API void InsertBarrier(VulkanCommandBuffer* commandBuffer, ResourceState newState) const;
    HYP_API void InsertBarrier(VulkanCommandBuffer* commandBuffer, ResourceState newState, ShaderModuleType shaderType) const;

    HYP_API virtual void CopyFrom(
        CommandBufferBase* commandBuffer,
        const GpuBufferBase* srcBuffer,
        SizeType count) override;

    HYP_API RendererResult CheckCanAllocate(SizeType size) const;

    HYP_API uint64 GetBufferDeviceAddress() const;

    HYP_API virtual RendererResult Create() override;
    HYP_API virtual RendererResult Destroy() override;

    HYP_API virtual RendererResult EnsureCapacity(
        SizeType minimumSize,
        bool* outSizeChanged = nullptr) override;

    HYP_API virtual RendererResult EnsureCapacity(
        SizeType minimumSize,
        SizeType alignment,
        bool* outSizeChanged = nullptr) override;

    HYP_API virtual void Memset(SizeType count, ubyte value) override;

    HYP_API virtual void Copy(SizeType count, const void* ptr) override;
    HYP_API virtual void Copy(SizeType offset, SizeType count, const void* ptr) override;

    HYP_API virtual void Read(SizeType count, void* outPtr) const override;
    HYP_API virtual void Read(SizeType offset, SizeType count, void* outPtr) const override;

    HYP_API virtual void Map() const override;
    HYP_API virtual void Unmap() const override;

private:
    RendererResult CheckCanAllocate(
        const VkBufferCreateInfo& bufferCreateInfo,
        const VmaAllocationCreateInfo& allocationCreateInfo,
        SizeType size) const;

    VmaAllocationCreateInfo GetAllocationCreateInfo() const;
    VkBufferCreateInfo GetBufferCreateInfo() const;

    VkBuffer m_handle = VK_NULL_HANDLE;

    VkBufferUsageFlags m_vkBufferUsageFlags = 0;
    VmaMemoryUsage m_vmaUsage = VMA_MEMORY_USAGE_UNKNOWN;
    VmaAllocationCreateFlags m_vmaAllocationCreateFlags = 0;
    VmaAllocation m_vmaAllocation = VK_NULL_HANDLE;

    mutable void* m_mapping = nullptr;
};

} // namespace hyperion

#endif // HYPERION_RENDERER_BACKEND_VULKAN_BUFFER_HPP
