#include "renderer_frame_handler.h"

namespace hyperion {
namespace renderer {

FrameHandler::FrameHandler(size_t num_frames, NextImageFunction next_image)
    : m_next_image(next_image),
      m_num_frames(num_frames),
      m_per_frame_data(num_frames),
      m_frame_index(0)
{
    //m_allocated_frames.resize(m_num_frames);
    //m_command_buffers.resize(m_num_frames);
}

FrameHandler::~FrameHandler()
{
}

Result FrameHandler::CreateFrames(Device *device)
{
    AssertThrow(m_per_frame_data.GetNumFrames() >= 1);
    
    for (size_t i = 0; i < m_per_frame_data.GetNumFrames(); i++) {
        auto frame = std::make_unique<Frame>();

        HYPERION_BUBBLE_ERRORS(frame->Create(device, non_owning_ptr(m_per_frame_data[i].GetCommandBuffer())));

        m_per_frame_data[i].SetFrame(std::move(frame));
    }

    DebugLog(LogType::Debug, "Allocated [%d] frames\n", m_per_frame_data.GetNumFrames());

    HYPERION_RETURN_OK;
}

Result FrameHandler::AcquireNextImage(Device *device, Swapchain *swapchain, VkResult *out_result)
{
    auto *frame = m_per_frame_data[m_frame_index].GetFrame();

    VkResult fence_result;
    do {
        fence_result = vkWaitForFences(device->GetDevice(), 1, &frame->fc_queue_submit, VK_TRUE, UINT64_MAX);
    } while (fence_result == VK_TIMEOUT);

    HYPERION_VK_CHECK(*out_result = fence_result);

    HYPERION_VK_CHECK(*out_result = vkResetFences(device->GetDevice(), 1, &frame->fc_queue_submit));

    HYPERION_VK_CHECK(*out_result = m_next_image(device, swapchain, frame, &m_frame_index));

    //HYPERION_VK_CHECK(*out_result = vkResetCommandBuffer(frame->command_buffer, 0));

    HYPERION_RETURN_OK;
}

Result FrameHandler::CreateCommandBuffers(Device *device, VkCommandPool pool)
{
    Result result = Result::OK;

    for (size_t i = 0; i < m_per_frame_data.GetNumFrames(); i++) {
        auto command_buffer = std::make_unique<CommandBuffer>(
            CommandBuffer::COMMAND_BUFFER_PRIMARY
        );

        HYPERION_PASS_ERRORS(command_buffer->Create(device, pool), result);

        m_per_frame_data[i].SetCommandBuffer(std::move(command_buffer));
    }

    DebugLog(LogType::Debug, "Allocated %d command buffers\n", m_per_frame_data.GetNumFrames());

    return result;
}

Result FrameHandler::Destroy(Device *device, VkCommandPool pool)
{
    Result result = Result::OK;

    for (size_t i = 0; i < m_per_frame_data.GetNumFrames(); i++) {
        HYPERION_PASS_ERRORS(m_per_frame_data[i].GetCommandBuffer()->Destroy(device, pool), result);
        HYPERION_PASS_ERRORS(m_per_frame_data[i].GetFrame()->Destroy(), result);
    }

    m_per_frame_data.Reset();

    return result;
}

} // namespace renderer
} // namespace hyperion