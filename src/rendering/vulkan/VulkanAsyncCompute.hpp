/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/AsyncCompute.hpp>
#include <rendering/vulkan/VulkanFence.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
class VulkanAsyncCompute final : public AsyncComputeBase
{
public:
    HYP_API VulkanAsyncCompute();
    HYP_API virtual ~VulkanAsyncCompute() override;

    virtual bool IsSupported() const override
    {
        return m_isSupported;
    }

    HYP_API RendererResult Create();
    HYP_API RendererResult Submit(VulkanFrame* frame);

    HYP_API RendererResult PrepareForFrame(VulkanFrame* frame);
    HYP_API RendererResult WaitForFence(VulkanFrame* frame);

private:
    FixedArray<VulkanCommandBufferRef, maxFramesInFlight> m_commandBuffers;
    FixedArray<VulkanFenceRef, maxFramesInFlight> m_fences;
    bool m_isSupported;
    bool m_isFallback;
};

} // namespace hyperion
