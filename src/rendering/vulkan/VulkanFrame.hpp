/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderFrame.hpp>

#include <rendering/vulkan/VulkanFrame.hpp>
#include <rendering/vulkan/VulkanSemaphore.hpp>

#include <rendering/RenderObject.hpp>

namespace hyperion {

class VulkanFence;
using VulkanFenceRef = RenderObjectHandle_Strong<VulkanFence>;

struct VulkanDeviceQueue;

class VulkanFrame final : public FrameBase
{
public:
    explicit VulkanFrame();
    explicit VulkanFrame(uint32 frameIndex);
    virtual ~VulkanFrame() override;

    HYP_API virtual RendererResult Create() override;
    HYP_API virtual RendererResult Destroy() override;

    HYP_API virtual RendererResult ResetFrameState() override;

    HYP_API RendererResult Submit(VulkanDeviceQueue* deviceQueue, const VulkanCommandBufferRef& commandBuffer);

    HYP_FORCE_INLINE const VulkanFenceRef& GetFence() const
    {
        return m_queueSubmitFence;
    }

    HYP_FORCE_INLINE VulkanSemaphoreChain& GetPresentSemaphores()
    {
        return m_presentSemaphores;
    }

    HYP_FORCE_INLINE const VulkanSemaphoreChain& GetPresentSemaphores() const
    {
        return m_presentSemaphores;
    }

    RendererResult RecreateFence();

private:
    VulkanSemaphoreChain m_presentSemaphores;
    VulkanFenceRef m_queueSubmitFence;
};

} // namespace hyperion
