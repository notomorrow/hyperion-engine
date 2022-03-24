#include "renderer_frame_handler.h"

namespace hyperion {
namespace renderer {

FrameHandler::FrameHandler(uint8_t num_images, NextImageFunction next_image)
    : m_per_frame_data(num_images),
      m_next_image(next_image),
      m_acquired_image_index(0)
{
    /* Will be set to 0 on first call to AcquireNextImage() */
    m_current_frame_index = (GetNumFrames() - 1) % GetNumFrames();  // NOLINT(cppcoreguidelines-prefer-member-initializer)
}

FrameHandler::~FrameHandler() = default;

Result FrameHandler::CreateFrames(Device *device)
{
    AssertThrow(m_per_frame_data.GetNumFrames() >= 1);
    
    for (uint32_t i = 0; i < m_per_frame_data.GetNumFrames(); i++) {
        auto frame = std::make_unique<Frame>();

        HYPERION_BUBBLE_ERRORS(frame->Create(device, non_owning_ptr(m_per_frame_data[i].Get<CommandBuffer>())));

        m_per_frame_data[i].Set<Frame>(std::move(frame));
    }

    DebugLog(LogType::Debug, "Allocated [%d] frames\n", m_per_frame_data.GetNumFrames());

    HYPERION_RETURN_OK;
}

Result FrameHandler::AcquireNextImage(Device *device, Swapchain *swapchain, VkResult *out_result)
{
    auto *frame = GetCurrentFrameData().Get<Frame>();

    VkResult fence_result{};

    do {
        fence_result = vkWaitForFences(device->GetDevice(), 1, &frame->fc_queue_submit, VK_TRUE, UINT64_MAX);
    } while (fence_result == VK_TIMEOUT);

    HYPERION_VK_CHECK(*out_result = fence_result);
    HYPERION_VK_CHECK(*out_result = vkResetFences(device->GetDevice(), 1, &frame->fc_queue_submit));

    HYPERION_VK_CHECK(*out_result = m_next_image(device, swapchain, frame, &m_acquired_image_index));

    m_current_frame_index = (m_current_frame_index + 1) % GetNumFrames();
    //HYPERION_VK_CHECK(*out_result = vkResetCommandBuffer(frame->command_buffer, 0));

    HYPERION_RETURN_OK;
}

Result FrameHandler::CreateCommandBuffers(Device *device, VkCommandPool pool)
{
    auto result = Result::OK;

    for (uint32_t i = 0; i < m_per_frame_data.GetNumFrames(); i++) {
        auto command_buffer = std::make_unique<CommandBuffer>(
            CommandBuffer::COMMAND_BUFFER_PRIMARY
        );

        HYPERION_PASS_ERRORS(command_buffer->Create(device, pool), result);

        m_per_frame_data[i].Set<CommandBuffer>(std::move(command_buffer));
    }

    DebugLog(LogType::Debug, "Allocated %d command buffers\n", m_per_frame_data.GetNumFrames());

    return result;
}

Result FrameHandler::Destroy(Device *device, VkCommandPool pool)
{
    auto result = Result::OK;

    for (uint32_t i = 0; i < m_per_frame_data.GetNumFrames(); i++) {
        HYPERION_PASS_ERRORS(m_per_frame_data[i].Get<CommandBuffer>()->Destroy(device, pool), result);
        HYPERION_PASS_ERRORS(m_per_frame_data[i].Get<Frame>()->Destroy(), result);
    }

    m_per_frame_data.Reset();

    return result;
}

} // namespace renderer
} // namespace hyperion