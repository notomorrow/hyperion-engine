/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once
#include <rendering/RenderCommandBuffer.hpp>
#include <rendering/RenderDevice.hpp>

#include <rendering/vulkan/VulkanGpuBuffer.hpp>
#include <rendering/vulkan/VulkanFence.hpp>

#include <core/containers/FixedArray.hpp>

#include <vulkan/vulkan.h>

#include <Types.hpp>

namespace hyperion {

class VulkanSemaphoreChain;
class VulkanRenderPass;
struct VulkanDeviceQueue;

class VulkanCommandBuffer final : public CommandBufferBase
{
public:
    HYP_API explicit VulkanCommandBuffer(VkCommandBufferLevel type);
    HYP_API virtual ~VulkanCommandBuffer() override;

    HYP_FORCE_INLINE VkCommandBuffer GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_FORCE_INLINE VkCommandPool GetVulkanCommandPool() const
    {
        return m_commandPool;
    }

    HYP_FORCE_INLINE VkCommandBufferLevel GetType() const
    {
        return m_type;
    }

    HYP_API virtual bool IsCreated() const override;

    HYP_API virtual RendererResult Create() override;
    HYP_API RendererResult Create(VkCommandPool commandPool);
    HYP_API virtual RendererResult Destroy() override;

    HYP_API RendererResult Begin(const VulkanRenderPass* renderPass = nullptr);
    HYP_API RendererResult End();
    HYP_API RendererResult Reset();
    HYP_API RendererResult SubmitPrimary(
        VulkanDeviceQueue* queue,
        VulkanFence* fence,
        VulkanSemaphoreChain* semaphoreChain);

    HYP_API RendererResult SubmitSecondary(VulkanCommandBuffer* primary);

    HYP_API virtual void BindVertexBuffer(const GpuBufferBase* buffer) override;
    HYP_API virtual void BindIndexBuffer(const GpuBufferBase* buffer, GpuElemType elemType = GET_UNSIGNED_INT) override;

    HYP_API virtual void DrawIndexed(
        uint32 numIndices,
        uint32 numInstances = 1,
        uint32 instanceIndex = 0) const override;

    HYP_API virtual void DrawIndexedIndirect(
        const GpuBufferBase* buffer,
        uint32 bufferOffset) const override;

    HYP_API void DebugMarkerBegin(const char* markerName) const;
    HYP_API void DebugMarkerEnd() const;

    template <class LambdaFunction>
    RendererResult Record(const VulkanRenderPass* renderPass, LambdaFunction&& fn)
    {
        HYPERION_BUBBLE_ERRORS(Begin(renderPass));

        RendererResult result = fn(this);

        HYPERION_PASS_ERRORS(End(), result);

        return result;
    }

private:
    VkCommandBufferLevel m_type;
    VkCommandBuffer m_handle;
    VkCommandPool m_commandPool;
};

} // namespace hyperion
