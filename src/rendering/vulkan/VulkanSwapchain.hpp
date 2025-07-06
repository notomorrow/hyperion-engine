/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once
#include <core/containers/Array.hpp>

#include <rendering/RenderSwapchain.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/vulkan/VulkanSemaphore.hpp>
#include <rendering/vulkan/VulkanFramebuffer.hpp>

#include <rendering/RenderStructs.hpp>

#include <Types.hpp>
#include <Constants.hpp>

#define HYP_ENABLE_VSYNC 0

namespace hyperion {

struct VulkanDeviceQueue;

class VulkanSwapchain final : public SwapchainBase
{
public:
    friend class VulkanInstance;

    HYP_API VulkanSwapchain();
    HYP_API virtual ~VulkanSwapchain() override;

    HYP_FORCE_INLINE VkSwapchainKHR GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_FORCE_INLINE const VulkanFrameRef& GetCurrentFrame() const
    {
        return m_frames[m_currentFrameIndex];
    }

    HYP_FORCE_INLINE const VulkanCommandBufferRef& GetCurrentCommandBuffer() const
    {
        return m_commandBuffers[m_currentFrameIndex];
    }

    HYP_FORCE_INLINE uint32 NumAcquiredImages() const
    {
        return uint32(m_images.Size());
    }

    HYP_API virtual bool IsCreated() const override;

    HYP_API void NextFrame();

    HYP_API RendererResult PrepareFrame(bool& outNeedsRecreate);
    HYP_API RendererResult PresentFrame(VulkanDeviceQueue* queue) const;

    HYP_API virtual RendererResult Create() override;
    HYP_API virtual RendererResult Destroy() override;

private:
    RendererResult ChooseSurfaceFormat();
    RendererResult ChoosePresentMode();
    RendererResult RetrieveSupportDetails();
    RendererResult RetrieveImageHandles();

    FixedArray<VulkanFrameRef, maxFramesInFlight> m_frames;
    FixedArray<VulkanCommandBufferRef, maxFramesInFlight> m_commandBuffers;

    VkSwapchainKHR m_handle;
    VkSurfaceKHR m_surface;
    VkSurfaceFormatKHR m_surfaceFormat;
    VkPresentModeKHR m_presentMode;
    VulkanSwapchainSupportDetails m_supportDetails;
};

} // namespace hyperion
