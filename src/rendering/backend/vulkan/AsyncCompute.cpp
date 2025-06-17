/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/vulkan/AsyncCompute.hpp>
#include <rendering/backend/vulkan/RendererFrame.hpp>
#include <rendering/backend/vulkan/RendererCommandBuffer.hpp>
#include <rendering/backend/vulkan/RendererComputePipeline.hpp>
#include <rendering/backend/vulkan/RendererDescriptorSet.hpp>
#include <rendering/backend/vulkan/RendererGpuBuffer.hpp>
#include <rendering/backend/vulkan/VulkanRenderBackend.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

extern IRenderBackend* g_renderBackend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return static_cast<VulkanRenderBackend*>(g_renderBackend);
}

VulkanAsyncCompute::VulkanAsyncCompute()
    : m_commandBuffers({ MakeRenderObject<VulkanCommandBuffer>(CommandBufferType::COMMAND_BUFFER_PRIMARY),
          MakeRenderObject<VulkanCommandBuffer>(CommandBufferType::COMMAND_BUFFER_PRIMARY) }),
      m_fences({ MakeRenderObject<VulkanFence>(),
          MakeRenderObject<VulkanFence>() }),
      m_isSupported(false),
      m_isFallback(false)
{
}

VulkanAsyncCompute::~VulkanAsyncCompute()
{
    SafeRelease(std::move(m_commandBuffers));
    SafeRelease(std::move(m_fences));
}

RendererResult VulkanAsyncCompute::Create()
{
    HYP_SCOPE;

    AssertThrow(GetRenderBackend()->GetDevice()->GetQueueFamilyIndices().IsComplete());

    VulkanDeviceQueue* queue = &GetRenderBackend()->GetDevice()->GetComputeQueue();

    m_isSupported = GetRenderBackend()->GetDevice()->GetQueueFamilyIndices().computeFamily.HasValue();

    if (!m_isSupported)
    {
        HYP_LOG(RenderingBackend, Warning, "Dedicated compute queue not supported, using graphics queue for compute operations");

        queue = &GetRenderBackend()->GetDevice()->GetGraphicsQueue();
    }

    for (const VulkanCommandBufferRef& commandBuffer : m_commandBuffers)
    {
        AssertThrow(commandBuffer.IsValid());

        HYPERION_BUBBLE_ERRORS(commandBuffer->Create(queue->commandPools[0]));
    }

    for (const VulkanFenceRef& fence : m_fences)
    {
        HYPERION_BUBBLE_ERRORS(fence->Create());
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanAsyncCompute::Submit(VulkanFrame* frame)
{
    HYP_SCOPE;

    const uint32 frameIndex = frame->GetFrameIndex();

    // @TODO: Call CmdList::Prepare to set descriptor sets to be used for the frame.

    HYPERION_BUBBLE_ERRORS(m_commandBuffers[frameIndex]->Begin());
    m_commandList.Execute(m_commandBuffers[frameIndex]);
    HYPERION_BUBBLE_ERRORS(m_commandBuffers[frameIndex]->End());

    VulkanDeviceQueue& computeQueue = GetRenderBackend()->GetDevice()->GetComputeQueue();

    return m_commandBuffers[frameIndex]->SubmitPrimary(&computeQueue, m_fences[frameIndex], nullptr);
}

RendererResult VulkanAsyncCompute::PrepareForFrame(VulkanFrame* frame)
{
    HYP_SCOPE;

    const uint32 frameIndex = frame->GetFrameIndex();

    HYPERION_BUBBLE_ERRORS(WaitForFence(frame));

    HYPERION_RETURN_OK;
}

RendererResult VulkanAsyncCompute::WaitForFence(VulkanFrame* frame)
{
    HYP_SCOPE;

    const uint32 frameIndex = frame->GetFrameIndex();

    RendererResult result = m_fences[frameIndex]->WaitForGPU();

    if (!result)
    {
        return result;
    }

    HYPERION_PASS_ERRORS(m_fences[frameIndex]->Reset(), result);

    return result;
}

} // namespace hyperion