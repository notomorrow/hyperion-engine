/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/vulkan/RendererFrame.hpp>
#include <rendering/backend/vulkan/RendererFence.hpp>
#include <rendering/backend/vulkan/RendererCommandBuffer.hpp>
#include <rendering/backend/vulkan/VulkanRenderBackend.hpp>

#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion {

extern IRenderBackend* g_renderBackend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return static_cast<VulkanRenderBackend*>(g_renderBackend);
}

VulkanFrame::VulkanFrame()
    : FrameBase(0),
      m_presentSemaphores({}, {})
{
}

VulkanFrame::VulkanFrame(uint32 frameIndex)
    : FrameBase(frameIndex),
      m_presentSemaphores(
          { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
          { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT })
{
    FrameBase::m_frameIndex = frameIndex;
}

VulkanFrame::~VulkanFrame()
{
    HYP_GFX_ASSERT(!m_queueSubmitFence.IsValid(), "fc_queue_submit should have been released");
}

RendererResult VulkanFrame::Create()
{
    HYPERION_BUBBLE_ERRORS(m_presentSemaphores.Create());

    m_queueSubmitFence = MakeRenderObject<VulkanFence>();
    HYPERION_BUBBLE_ERRORS(m_queueSubmitFence->Create());

    HYPERION_RETURN_OK;
}

RendererResult VulkanFrame::Destroy()
{
    RendererResult result;

    HYPERION_PASS_ERRORS(m_presentSemaphores.Destroy(), result);

    SafeRelease(std::move(m_queueSubmitFence));

    return result;
}

RendererResult VulkanFrame::ResetFrameState()
{
    RendererResult result;

    HYPERION_PASS_ERRORS(m_queueSubmitFence->Reset(), result);

#ifdef HYP_DESCRIPTOR_SET_TRACK_FRAME_USAGE
    for (DescriptorSetBase* descriptorSet : m_usedDescriptorSets)
    {
        auto it = descriptorSet->GetCurrentFrames().FindAs(this);
        if (it != descriptorSet->GetCurrentFrames().End())
        {
            // Remove the current frame from the descriptor set's current frames
            // This is necessary to ensure that the descriptor set is not used in the next frame
            descriptorSet->GetCurrentFrames().Erase(it);
        }
    }
#endif

    m_usedDescriptorSets.Clear();

    if (OnFrameEnd.AnyBound())
    {
        OnFrameEnd(this);
        OnFrameEnd.RemoveAllDetached();
    }

    return result;
}

RendererResult VulkanFrame::Submit(VulkanDeviceQueue* deviceQueue, const VulkanCommandBufferRef& commandBuffer)
{
    m_commandList.Prepare(this);

    UpdateUsedDescriptorSets();

    if (OnPresent.AnyBound())
    {
        OnPresent(this);
        OnPresent.RemoveAllDetached();
    }

    commandBuffer->Begin();
    m_commandList.Execute(commandBuffer);
    commandBuffer->End();

    return commandBuffer->SubmitPrimary(deviceQueue, m_queueSubmitFence, &m_presentSemaphores);
}

RendererResult VulkanFrame::RecreateFence()
{
    if (m_queueSubmitFence.IsValid())
    {
        SafeRelease(std::move(m_queueSubmitFence));
    }

    m_queueSubmitFence = MakeRenderObject<VulkanFence>();
    return m_queueSubmitFence->Create();
}

} // namespace hyperion