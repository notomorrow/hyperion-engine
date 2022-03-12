#include "renderer_command_buffer.h"

namespace hyperion {
namespace renderer {

CommandBuffer::CommandBuffer(Type type)
    : m_type(type),
      m_command_buffer(nullptr),
      m_signal(nullptr)
{
}

CommandBuffer::~CommandBuffer()
{
    AssertThrowMsg(m_command_buffer == nullptr, "command buffer should have been destroyed");
    AssertThrowMsg(m_signal == nullptr, "signal should have been destroyed");
}

Result CommandBuffer::Create(Device *device, VkCommandPool command_pool)
{
    VkSemaphoreCreateInfo semaphore_info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    HYPERION_VK_CHECK_MSG(
        vkCreateSemaphore(device->GetDevice(), &semaphore_info, nullptr, &m_signal),
        "Failed to create semaphore"
    );

    VkCommandBufferLevel level;

    switch (m_type) {
    case COMMAND_BUFFER_PRIMARY:
        level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        break;
    case COMMAND_BUFFER_SECONDARY:
        level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        break;
    default:
        AssertThrowMsg(0, "Unsupported command buffer type");
    }

    /*VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    HYPERION_VK_CHECK_MSG(
        vkCreateFence(device->GetDevice(), &fence_info, nullptr, &m_fence),
        "Failed to create fence"
    );*/

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = level;
    alloc_info.commandPool = command_pool;
    alloc_info.commandBufferCount = 1;

    HYPERION_VK_CHECK_MSG(
        vkAllocateCommandBuffers(device->GetDevice(), &alloc_info, &m_command_buffer),
        "Failed to allocate command buffer"
    );

    HYPERION_RETURN_OK;
}

Result CommandBuffer::Destroy(Device *device, VkCommandPool command_pool)
{
    vkDestroySemaphore(device->GetDevice(), m_signal, nullptr);
    m_signal = nullptr;

    vkFreeCommandBuffers(device->GetDevice(), command_pool, 1, &m_command_buffer);
    m_command_buffer = nullptr;

    HYPERION_RETURN_OK;
}

Result CommandBuffer::Begin(Device *device, const RenderPass *render_pass)
{
    HYPERION_VK_CHECK_MSG(
        vkResetCommandBuffer(m_command_buffer, 0),
        "Failed to reset command buffer"
    );

    VkCommandBufferInheritanceInfo inheritance_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO };
    inheritance_info.renderPass = render_pass->GetRenderPass();
    inheritance_info.subpass = 0;
    inheritance_info.framebuffer = VK_NULL_HANDLE;

    VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};

    if (m_type == COMMAND_BUFFER_SECONDARY) {
        AssertThrowMsg(render_pass != nullptr, "Render pass not provided for secondary command buffer!");

        begin_info.pInheritanceInfo = &inheritance_info;

        /* todo: make another flag */
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    }

    HYPERION_VK_CHECK_MSG(
        vkBeginCommandBuffer(m_command_buffer, &begin_info),
        "Failed to begin command buffer"
    );

    HYPERION_RETURN_OK;
}

Result CommandBuffer::End(Device *device)
{
    HYPERION_VK_CHECK_MSG(
        vkEndCommandBuffer(m_command_buffer),
        "Failed to end command buffer"
    );

    HYPERION_RETURN_OK;
}

Result CommandBuffer::Reset(Device *device)
{
    HYPERION_VK_CHECK_MSG(
        vkResetCommandBuffer(m_command_buffer, 0),
        "Failed to reset command buffer"
    );

    HYPERION_RETURN_OK;
}


Result CommandBuffer::SubmitSecondary(VkCommandBuffer primary,
    const std::vector<std::unique_ptr<CommandBuffer>> &command_buffers)
{
    AssertThrow(command_buffers.size() < 16);

    VkCommandBuffer *command_buffers_raw = (VkCommandBuffer *)alloca(sizeof(VkCommandBuffer) * command_buffers.size());

    for (uint8_t i = 0; i < command_buffers.size(); i++) {
        command_buffers_raw[i] = command_buffers[i]->GetCommandBuffer();
    }

    return SubmitSecondary(primary,
        command_buffers_raw,
        command_buffers.size());
}

Result CommandBuffer::SubmitSecondary(VkCommandBuffer primary,
    VkCommandBuffer *command_buffers,
    size_t num_command_buffers)
{
    vkCmdExecuteCommands(primary, uint32_t(num_command_buffers), command_buffers);

    HYPERION_RETURN_OK;
}

Result CommandBuffer::SubmitPrimary(VkQueue queue,
    const std::vector<std::unique_ptr<CommandBuffer>> &command_buffers,
    VkFence fence,
    VkSemaphore *wait_semaphores,
    size_t num_wait_semaphores)
{
    AssertThrow(command_buffers.size() < 16);

    VkCommandBuffer *command_buffers_raw = (VkCommandBuffer *)alloca(sizeof(VkCommandBuffer) * command_buffers.size());
    VkSemaphore *signal_semaphores = (VkSemaphore *)alloca(sizeof(VkSemaphore) * command_buffers.size());

    for (uint8_t i = 0; i < command_buffers.size(); i++) {
        command_buffers_raw[i] = command_buffers[i]->GetCommandBuffer();
        signal_semaphores[i] = command_buffers[i]->GetSemaphore();
    }

    return SubmitPrimary(queue,
        command_buffers_raw,
        command_buffers.size(),
        fence,
        wait_semaphores, num_wait_semaphores,
        signal_semaphores, command_buffers.size());
}

Result CommandBuffer::SubmitPrimary(VkQueue queue,
    const std::vector<std::unique_ptr<CommandBuffer>> &command_buffers,
    VkFence fence,
    VkSemaphore *wait_semaphores,
    size_t num_wait_semaphores,
    VkSemaphore *signal_semaphores,
    size_t num_signal_semaphores)
{
    AssertThrow(command_buffers.size() < 16);

    VkCommandBuffer *command_buffers_raw = (VkCommandBuffer*)alloca(sizeof(VkCommandBuffer) * command_buffers.size());

    for (uint8_t i = 0; i < command_buffers.size(); i++) {
        command_buffers_raw[i] = command_buffers[i]->GetCommandBuffer();
    }

    return SubmitPrimary(queue,
        command_buffers_raw, 
        command_buffers.size(),
        fence,
        wait_semaphores, num_wait_semaphores,
        signal_semaphores, num_signal_semaphores);
}

Result CommandBuffer::SubmitPrimary(VkQueue queue,
    VkCommandBuffer *command_buffers,
    size_t num_command_buffers,
    VkFence fence,
    VkSemaphore *wait_semaphores,
    size_t num_wait_semaphores,
    VkSemaphore *signal_semaphores,
    size_t num_signal_semaphores)
{
    VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO};

    std::vector<VkPipelineStageFlags> wait_stages;

    if (num_wait_semaphores != 0) {
        wait_stages.resize(num_wait_semaphores);

        for (int i = 0; i < num_wait_semaphores; i++) {
            wait_stages[i] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // TODO: take in parameter
        }
    }

    submit_info.waitSemaphoreCount = num_wait_semaphores;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.signalSemaphoreCount = num_signal_semaphores;
    submit_info.pSignalSemaphores = signal_semaphores;
    submit_info.pWaitDstStageMask = wait_stages.data();
    submit_info.commandBufferCount = num_command_buffers;
    submit_info.pCommandBuffers = command_buffers;

    HYPERION_VK_CHECK_MSG(
        vkQueueSubmit(queue, 1, &submit_info, fence),
        "Failed to submit command"
    );

    HYPERION_RETURN_OK;
}

Result CommandBuffer::SubmitPrimary(VkQueue queue, VkFence fence, VkSemaphore *wait_semaphores, size_t num_wait_semaphores)
{
    VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO};

    std::vector<VkPipelineStageFlags> wait_stages;

    if (num_wait_semaphores != 0) {
        wait_stages.resize(num_wait_semaphores);

        for (int i = 0; i < num_wait_semaphores; i++) {
            wait_stages[i] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // TODO: take in parameter
        }
    }

    submit_info.waitSemaphoreCount = num_wait_semaphores;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &m_signal;
    submit_info.pWaitDstStageMask = wait_stages.data();
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &m_command_buffer;

    HYPERION_VK_CHECK_MSG(
        vkQueueSubmit(queue, 1, &submit_info, fence),
        "Failed to submit command"
    );

    HYPERION_RETURN_OK;
}

Result CommandBuffer::SubmitSecondary(VkCommandBuffer primary)
{
    vkCmdExecuteCommands(primary, 1, &m_command_buffer);

    HYPERION_RETURN_OK;
}


} // namespace renderer
} // namespace hyperion