#include "renderer_frame.h"

namespace hyperion {
namespace renderer {
Frame::Frame()
    : creation_device(nullptr),
      command_buffer(nullptr),
      fc_queue_submit(nullptr)
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

    vkDestroyFence(this->creation_device->GetDevice(), this->fc_queue_submit, nullptr);
    this->fc_queue_submit = nullptr;

    return result;
}

void Frame::BeginCapture()
{
    auto result = this->command_buffer->Begin(this->creation_device.get());
    AssertThrowMsg(result, "Failed to start recording command buffer: %s", result.message);
}

void Frame::EndCapture()
{
    auto result = this->command_buffer->End(this->creation_device.get());
    AssertThrowMsg(result, "Failed to finish recording command buffer: %s", result.message);
}

void Frame::Submit(VkQueue queue_submit, SemaphoreChain *semaphore_chain)
{
    auto result = this->command_buffer->SubmitPrimary(queue_submit, this->fc_queue_submit, semaphore_chain);
    AssertThrowMsg(result, "Failed to submit draw command buffer: %s", result.message);
}

} // namespace renderer
} // namespace hyperion