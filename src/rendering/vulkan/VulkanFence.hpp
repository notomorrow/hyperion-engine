/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderObject.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
class VulkanFence final : public RenderObject<VulkanFence>
{
public:
    VulkanFence();
    virtual ~VulkanFence() override;

    HYP_FORCE_INLINE VkFence GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_FORCE_INLINE VkResult GetLastFrameResult() const
    {
        return m_lastFrameResult;
    }

    RendererResult Create();
    RendererResult Destroy();
    RendererResult WaitForGPU(bool timeoutLoop = false);
    RendererResult Reset();

private:
    VkFence m_handle = VK_NULL_HANDLE;
    VkResult m_lastFrameResult = VK_SUCCESS;
};

} // namespace hyperion
