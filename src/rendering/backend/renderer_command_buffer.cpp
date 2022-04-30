#include "renderer_command_buffer.h"

namespace hyperion {
namespace renderer {

CommandBuffer::CommandBuffer(Type type)
    : m_type(type),
      m_command_buffer(nullptr)
{
}

CommandBuffer::~CommandBuffer()
{
    AssertThrowMsg(m_command_buffer == nullptr, "command buffer should have been destroyed");
}

Result CommandBuffer::Create(Device *device, VkCommandPool command_pool)
{
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

    VkCommandBufferAllocateInfo alloc_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
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
    auto result = Result::OK;

    vkFreeCommandBuffers(device->GetDevice(), command_pool, 1, &m_command_buffer);
    m_command_buffer = nullptr;

    return result;
}

Result CommandBuffer::Begin(Device *device, const RenderPass *render_pass)
{
    VkCommandBufferInheritanceInfo inheritance_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO};
    inheritance_info.subpass = 0;
    inheritance_info.framebuffer = VK_NULL_HANDLE;

    VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};

    if (m_type == COMMAND_BUFFER_SECONDARY) {
        AssertThrowMsg(render_pass != nullptr, "Render pass not provided for secondary command buffer!");
        inheritance_info.renderPass = render_pass->GetRenderPass();

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

Result CommandBuffer::SubmitSecondary(CommandBuffer *primary,
    const std::vector<std::unique_ptr<CommandBuffer>> &command_buffers)
{
    for (auto &command_buffer : command_buffers) {
        vkCmdExecuteCommands(primary->GetCommandBuffer(), 1, &command_buffer->GetCommandBuffer());
    }

    HYPERION_RETURN_OK;
}

Result CommandBuffer::SubmitSecondary(VkCommandBuffer primary,
    VkCommandBuffer *command_buffers,
    size_t num_command_buffers)
{
    vkCmdExecuteCommands(primary, uint32_t(num_command_buffers), command_buffers);

    HYPERION_RETURN_OK;
}

Result CommandBuffer::SubmitPrimary(VkQueue queue, VkFence fence, SemaphoreChain *semaphore_chain)
{
    VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO};

    if (semaphore_chain != nullptr) {
        submit_info.waitSemaphoreCount = uint32_t(semaphore_chain->m_wait_semaphores_view.size());
        submit_info.pWaitSemaphores = semaphore_chain->m_wait_semaphores_view.data();
        submit_info.signalSemaphoreCount = uint32_t(semaphore_chain->m_signal_semaphores_view.size());
        submit_info.pSignalSemaphores = semaphore_chain->m_signal_semaphores_view.data();
        submit_info.pWaitDstStageMask = semaphore_chain->m_wait_semaphores_stage_view.data();
    } else {
        submit_info.waitSemaphoreCount = uint32_t(0);
        submit_info.pWaitSemaphores = nullptr;
        submit_info.signalSemaphoreCount = uint32_t(0);
        submit_info.pSignalSemaphores = nullptr;
        submit_info.pWaitDstStageMask = nullptr;
    }

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