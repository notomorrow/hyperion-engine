/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererFence.hpp>
#include <rendering/backend/RendererQueue.hpp>
#include <rendering/backend/RenderObject.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

template <>
Frame<Platform::VULKAN>::Frame()
    : m_frame_index(0),
      m_command_buffer(CommandBufferRef<Platform::VULKAN>::unset),
      m_present_semaphores({}, {})
{
}

template <>
Frame<Platform::VULKAN>::Frame(uint32 frame_index)
    : m_frame_index(frame_index),
      m_command_buffer(CommandBufferRef<Platform::VULKAN>::unset),
      m_present_semaphores(
          { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
          { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }
      )
{
}

template <>
Frame<Platform::VULKAN>::Frame(Frame &&other) noexcept
    : m_frame_index(other.m_frame_index),
      m_command_buffer(std::move(other.m_command_buffer)),
      m_queue_submit_fence(std::move(other.m_queue_submit_fence)),
      m_present_semaphores(std::move(other.m_present_semaphores))
{
    other.m_frame_index = 0;
}

template <>
Frame<Platform::VULKAN>::~Frame()
{
    AssertThrowMsg(!m_queue_submit_fence.IsValid(), "fc_queue_submit should have been released");
}

template <>
RendererResult Frame<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device, CommandBufferRef<Platform::VULKAN> cmd)
{
    m_command_buffer = std::move(cmd);
    
    HYPERION_BUBBLE_ERRORS(m_present_semaphores.Create(device));

    m_queue_submit_fence = MakeRenderObject<Fence<Platform::VULKAN>>();
    HYPERION_BUBBLE_ERRORS(m_queue_submit_fence->Create(device));

    HYPERION_RETURN_OK;
}

template <>
RendererResult Frame<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    RendererResult result;

    HYPERION_PASS_ERRORS(m_present_semaphores.Destroy(device), result);

    SafeRelease(std::move(m_command_buffer));
    SafeRelease(std::move(m_queue_submit_fence));

    return result;
}

template <>
RendererResult Frame<Platform::VULKAN>::BeginCapture(Device<Platform::VULKAN> *device)
{
    return m_command_buffer->Begin(device);
}

template <>
RendererResult Frame<Platform::VULKAN>::EndCapture(Device<Platform::VULKAN> *device)
{
    return m_command_buffer->End(device);
}

template <>
RendererResult Frame<Platform::VULKAN>::Submit(DeviceQueue<Platform::VULKAN> *queue)
{
    return m_command_buffer->SubmitPrimary(
        queue,
        m_queue_submit_fence,
        &m_present_semaphores
    );
}

template <>
RendererResult Frame<Platform::VULKAN>::RecreateFence(Device<Platform::VULKAN> *device)
{
    if (m_queue_submit_fence.IsValid()) {
        SafeRelease(std::move(m_queue_submit_fence));
    }

    m_queue_submit_fence = MakeRenderObject<Fence<Platform::VULKAN>>();
    return m_queue_submit_fence->Create(device);
}

} // namespace platform
} // namespace renderer
} // namespace hyperion