#include "renderer_frame.h"

namespace hyperion {
namespace renderer {
Frame::Frame()
    : command_buffer(nullptr),
      fc_queue_submit(nullptr),
      present_semaphores(
          { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
          { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }
      )
{
}

Frame::~Frame()
{
    AssertExitMsg(fc_queue_submit == nullptr, "fc_queue_submit should have been destroyed");
}

Result Frame::Create(Device *device, const non_owning_ptr<CommandBuffer> &cmd)
{
    this->command_buffer = cmd;
    
    HYPERION_BUBBLE_ERRORS(present_semaphores.Create(device));

    fc_queue_submit = std::make_unique<Fence>(true);

    HYPERION_BUBBLE_ERRORS(fc_queue_submit->Create(device));

    DebugLog(LogType::Debug, "Create Sync objects!\n");

    HYPERION_RETURN_OK;
}

Result Frame::Destroy(Device *device)
{
    auto result = Result::OK;

    HYPERION_PASS_ERRORS(present_semaphores.Destroy(device), result);

    AssertThrow(fc_queue_submit != nullptr);

    HYPERION_PASS_ERRORS(fc_queue_submit->Destroy(device), result);
    fc_queue_submit = nullptr;

    return result;
}

Result Frame::BeginCapture(Device *device)
{
    return command_buffer->Begin(device);
}

Result Frame::EndCapture(Device *device)
{
    return command_buffer->End(device);
}

Result Frame::Submit(Queue *queue)
{
    return command_buffer->SubmitPrimary(queue->queue, fc_queue_submit->GetFence(), &present_semaphores);
}

} // namespace renderer
} // namespace hyperion