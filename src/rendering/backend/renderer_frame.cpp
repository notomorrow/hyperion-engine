#include "renderer_frame.h"

namespace hyperion {
namespace renderer {
Frame::Frame()
    : creation_device(nullptr),
      command_buffer(nullptr),
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
    this->creation_device = non_owning_ptr(device);
    this->command_buffer = cmd;

    return this->CreateSyncObjects();
}

Result Frame::Destroy()
{
    return this->DestroySyncObjects();
}

Result Frame::CreateSyncObjects()
{
    AssertThrow(this->creation_device != nullptr);

    VkDevice rd_device = this->creation_device->GetDevice();

    HYPERION_BUBBLE_ERRORS(present_semaphores.Create(this->creation_device.get()));

    VkFenceCreateInfo fence_info{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    HYPERION_VK_CHECK_MSG(
        vkCreateFence(rd_device, &fence_info, nullptr, &this->fc_queue_submit),
        "Error creating render fence!"
    );

    DebugLog(LogType::Debug, "Create Sync objects!\n");

    HYPERION_RETURN_OK;
}

Result Frame::DestroySyncObjects()
{
    Result result = Result::OK;

    AssertThrow(this->creation_device != nullptr);

    HYPERION_PASS_ERRORS(present_semaphores.Destroy(this->creation_device.get()), result);

    vkDestroyFence(this->creation_device->GetDevice(), this->fc_queue_submit, nullptr);
    this->fc_queue_submit = nullptr;

    return result;
}

Result Frame::BeginCapture()
{
    return command_buffer->Begin(this->creation_device.get());
}

Result Frame::EndCapture()
{
    return command_buffer->End(this->creation_device.get());
}

Result Frame::Submit(Queue *queue)
{
    return command_buffer->SubmitPrimary(queue->queue, fc_queue_submit, &present_semaphores);
}

} // namespace renderer
} // namespace hyperion