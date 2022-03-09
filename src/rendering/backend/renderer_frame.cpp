#include "renderer_frame.h"

namespace hyperion {
namespace renderer {
Frame::Frame()
    : creation_device(nullptr),
      command_buffer(nullptr),
      sp_swap_acquire(nullptr),
      sp_swap_release(nullptr),
      fc_queue_submit(nullptr)
{
}

Frame::~Frame()
{
    AssertExitMsg(sp_swap_acquire == nullptr, "sp_swap_acquire should have been destroyed");
    AssertExitMsg(sp_swap_release == nullptr, "sp_swap_release should have been destroyed");
    AssertExitMsg(fc_queue_submit == nullptr, "fc_queue_submit should have been destroyed");
}

Result Frame::Create(non_owning_ptr<Device> device, VkCommandBuffer cmd)
{
    this->creation_device = device;
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
    VkSemaphoreCreateInfo semaphore_info{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

    HYPERION_VK_CHECK_MSG(
        vkCreateSemaphore(rd_device, &semaphore_info, nullptr, &this->sp_swap_acquire),
        "Error creating render swap acquire semaphore!"
    );

    HYPERION_VK_CHECK_MSG(
        vkCreateSemaphore(rd_device, &semaphore_info, nullptr, &this->sp_swap_release),
        "Error creating render swap release semaphore!"
    );

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

    vkDestroySemaphore(this->creation_device->GetDevice(), this->sp_swap_acquire, nullptr);
    this->sp_swap_acquire = nullptr;

    vkDestroySemaphore(this->creation_device->GetDevice(), this->sp_swap_release, nullptr);
    this->sp_swap_release = nullptr;

    vkDestroyFence(this->creation_device->GetDevice(), this->fc_queue_submit, nullptr);
    this->fc_queue_submit = nullptr;

    return result;
}

void Frame::BeginCapture()
{
    //VkCommandBuffer *cmd = &this->command_buffers[frame_index];
    VkCommandBufferBeginInfo begin_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    //begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    begin_info.pInheritanceInfo = nullptr;
    /* Begin recording our command buffer */
    auto result = vkBeginCommandBuffer(this->command_buffer, &begin_info);
    AssertThrowMsg(result == VK_SUCCESS, "Failed to start recording command buffer!\n");
}

void Frame::EndCapture()
{
    auto result = vkEndCommandBuffer(this->command_buffer);
    AssertThrowMsg(result == VK_SUCCESS, "Failed to record command buffer!\n");
}

void Frame::Submit(VkQueue queue_submit)
{
    VkSubmitInfo submit_info{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &this->sp_swap_acquire;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &this->sp_swap_release;

    submit_info.pWaitDstStageMask = wait_stages;

    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &this->command_buffer;

    auto result = vkQueueSubmit(queue_submit, 1, &submit_info, this->fc_queue_submit);
    AssertThrowMsg(result == VK_SUCCESS, "Failed to submit draw command buffer!\n");
}

} // namespace renderer
} // namespace hyperion