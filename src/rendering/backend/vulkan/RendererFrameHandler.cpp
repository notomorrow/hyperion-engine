#include <rendering/backend/RendererFrameHandler.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererQueue.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <math/MathUtil.hpp>
#include <Constants.hpp>

namespace hyperion {
namespace renderer {

namespace platform {

template <>
FrameHandler<Platform::VULKAN>::FrameHandler(uint num_frames, NextImageFunction next_image)
    : m_next_image(next_image),
      m_acquired_image_index(0),
      m_current_frame_index(0)
{
}

template <>
FrameHandler<Platform::VULKAN>::~FrameHandler() = default;

template <>
Result FrameHandler<Platform::VULKAN>::CreateFrames(Device<Platform::VULKAN> *device, DeviceQueue *queue)
{
    for (uint i = 0; i < m_frames.Size(); i++) {
        auto command_buffer = MakeRenderObject<renderer::CommandBuffer, Platform::VULKAN>(CommandBufferType::COMMAND_BUFFER_PRIMARY);

        VkCommandPool pool = queue->command_pools[0];
        AssertThrow(pool != VK_NULL_HANDLE);

        HYPERION_BUBBLE_ERRORS(command_buffer->Create(device, pool));

        auto frame = MakeRenderObject<renderer::Frame, Platform::VULKAN>(i);

        HYPERION_BUBBLE_ERRORS(frame->Create(
            device,
            std::move(command_buffer)
        ));

        m_frames[i] = std::move(frame);
    }

    HYPERION_RETURN_OK;
}

template <>
Result FrameHandler<Platform::VULKAN>::PrepareFrame(
    Device<Platform::VULKAN> *device,
    Swapchain<Platform::VULKAN> *swapchain
)
{
    const FrameRef<Platform::VULKAN> &frame = GetCurrentFrame();

    VkResult fence_result;

    frame->GetFence()->WaitForGPU(device, true, &fence_result);

    if (fence_result == VK_SUBOPTIMAL_KHR || fence_result == VK_ERROR_OUT_OF_DATE_KHR) {
        DebugLog(LogType::Debug, "Waiting -- image result was %d\n", fence_result);

        vkDeviceWaitIdle(device->GetDevice());

        /* TODO: regenerate framebuffers and swapchain */
    }

    HYPERION_VK_CHECK(fence_result);
    HYPERION_BUBBLE_ERRORS(frame->GetFence()->Reset(device));

    HYPERION_BUBBLE_ERRORS(m_next_image(device, swapchain, frame, &m_acquired_image_index));

    HYPERION_RETURN_OK;
}

template <>
void FrameHandler<Platform::VULKAN>::NextFrame()
{
    m_current_frame_index = (m_current_frame_index + 1) % max_frames_in_flight;
}

template <>
Result FrameHandler<Platform::VULKAN>::PresentFrame(
    DeviceQueue *queue,
    Swapchain<Platform::VULKAN> *swapchain
) const
{
    const FrameRef<Platform::VULKAN> &frame = GetCurrentFrame();

    const auto &signal_semaphores = frame->GetPresentSemaphores().GetSignalSemaphoresView();

    VkPresentInfoKHR present_info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};

    present_info.waitSemaphoreCount = uint32(signal_semaphores.size());
    present_info.pWaitSemaphores = signal_semaphores.data();

    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swapchain->swapchain;
    present_info.pImageIndices = &m_acquired_image_index;
    present_info.pResults = nullptr;

    HYPERION_VK_CHECK(vkQueuePresentKHR(queue->queue, &present_info));

    HYPERION_RETURN_OK;
}

template <>
Result FrameHandler<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    SafeRelease(std::move(m_frames));

    return Result::OK;
}

} // namespace platform
} // namespace renderer
} // namespace hyperion
