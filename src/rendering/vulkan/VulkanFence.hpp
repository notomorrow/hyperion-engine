/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderObject.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
class VulkanFence final : public RenderObject<VulkanFence>
{
public:
    HYP_API VulkanFence();
    HYP_API virtual ~VulkanFence() override;

    HYP_FORCE_INLINE VkFence GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_FORCE_INLINE VkResult GetLastFrameResult() const
    {
        return m_lastFrameResult;
    }

    HYP_API RendererResult Create();
    HYP_API RendererResult Destroy();
    HYP_API RendererResult WaitForGPU(bool timeoutLoop = false);
    HYP_API RendererResult Reset();

private:
    VkFence m_handle = VK_NULL_HANDLE;
    VkResult m_lastFrameResult = VK_SUCCESS;
};

} // namespace hyperion
