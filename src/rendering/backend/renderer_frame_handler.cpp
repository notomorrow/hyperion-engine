#include "renderer_frame_handler.h"

namespace hyperion {
namespace renderer {

FrameHandler::FrameHandler(NextImageFunction_t next_image)
    : m_next_image(next_image),
      m_image_count(0),
      m_image_index(0)
{
}

FrameHandler::~FrameHandler()
{
}

Result FrameHandler::AllocateFrames(Device *device)
{
    AssertThrow(m_image_count >= 1);
    AssertThrowMsg(m_command_buffers.size() >= m_image_count,
        "Insufficient command buffers\n");

    m_allocated_frames.resize(m_image_count);
    DebugLog(LogType::Debug, "Allocating [%d] frames\n", m_image_count);

    for (size_t i = 0; i < m_image_count; i++) {
        auto frame = std::make_unique<Frame>();

        HYPERION_BUBBLE_ERRORS(frame->Create(device, m_command_buffers[i]->GetCommandBuffer()));

        m_allocated_frames[i] = std::move(frame);
    }

    HYPERION_RETURN_OK;
}

Result FrameHandler::AcquireNextImage(Device *device, Swapchain *swapchain)
{
    const auto &frame = m_allocated_frames[m_image_index];

    HYPERION_VK_CHECK(vkWaitForFences(device->GetDevice(), 1, &frame->fc_queue_submit, true, UINT64_MAX));
    HYPERION_VK_CHECK(vkResetFences(device->GetDevice(), 1, &frame->fc_queue_submit));

    m_next_image(device, swapchain, frame.get(), &m_image_index);

    HYPERION_VK_CHECK(vkResetCommandBuffer(frame->command_buffer, 0));

    HYPERION_RETURN_OK;
}

Frame *FrameHandler::Next()
{
    AssertThrow(m_allocated_frames.size() == m_image_count);

    ++m_image_index;

    if (m_image_index >= m_image_count) {
        m_image_index = 0;
    }

    return GetCurrentFrame();
}

} // namespace renderer
} // namespace hyperion