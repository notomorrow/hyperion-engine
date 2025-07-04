/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/vulkan/RendererFence.hpp>
#include <rendering/backend/vulkan/VulkanRenderBackend.hpp>

#include <rendering/backend/RendererDevice.hpp>

#define DEFAULT_FENCE_TIMEOUT 100000000000

namespace hyperion {

extern IRenderBackend* g_renderBackend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return static_cast<VulkanRenderBackend*>(g_renderBackend);
}

VulkanFence::VulkanFence()
    : m_handle(VK_NULL_HANDLE),
      m_lastFrameResult(VK_SUCCESS)
{
}

VulkanFence::~VulkanFence()
{
    HYP_GFX_ASSERT(m_handle == VK_NULL_HANDLE, "fence should have been destroyed");
}

RendererResult VulkanFence::Create()
{
    HYP_GFX_ASSERT(m_handle == VK_NULL_HANDLE);

    // Create fence to ensure that the command buffer has finished executing
    VkFenceCreateInfo fenceCreateInfo { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    HYPERION_VK_CHECK(vkCreateFence(GetRenderBackend()->GetDevice()->GetDevice(), &fenceCreateInfo, nullptr, &m_handle));

    HYPERION_RETURN_OK;
}

RendererResult VulkanFence::Destroy()
{
    if (m_handle != VK_NULL_HANDLE)
    {
        vkDestroyFence(GetRenderBackend()->GetDevice()->GetDevice(), m_handle, nullptr);
        m_handle = VK_NULL_HANDLE;
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanFence::WaitForGPU(bool timeoutLoop)
{
    HYP_GFX_ASSERT(m_handle != VK_NULL_HANDLE);

    VkResult vkResult;

    do
    {
        vkResult = vkWaitForFences(GetRenderBackend()->GetDevice()->GetDevice(), 1, &m_handle, VK_TRUE, DEFAULT_FENCE_TIMEOUT);
    }
    while (vkResult == VK_TIMEOUT && timeoutLoop);

    HYPERION_VK_CHECK(vkResult);

    m_lastFrameResult = vkResult;

    HYPERION_RETURN_OK;
}

RendererResult VulkanFence::Reset()
{
    HYPERION_VK_CHECK(vkResetFences(GetRenderBackend()->GetDevice()->GetDevice(), 1, &m_handle));

    HYPERION_RETURN_OK;
}

} // namespace hyperion