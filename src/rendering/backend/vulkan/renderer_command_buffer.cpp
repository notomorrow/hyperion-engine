#include "renderer_command_buffer.h"
#include "renderer_compute_pipeline.h"
#include "renderer_graphics_pipeline.h"

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
    inheritance_info.subpass     = 0;
    inheritance_info.framebuffer = VK_NULL_HANDLE;

    VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};

    if (m_type == COMMAND_BUFFER_SECONDARY) {
        AssertThrowMsg(render_pass != nullptr, "Render pass not provided for secondary command buffer!");
        inheritance_info.renderPass = render_pass->GetHandle();

        begin_info.pInheritanceInfo = &inheritance_info;
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

Result CommandBuffer::SubmitSecondary(
    CommandBuffer *primary,
    const std::vector<std::unique_ptr<CommandBuffer>> &command_buffers
)
{
    const VkCommandBuffer *vk_command_buffers = static_cast<VkCommandBuffer *>(alloca(sizeof(VkCommandBuffer) * command_buffers.size()));

    vkCmdExecuteCommands(
        primary->GetCommandBuffer(),
        uint32_t(command_buffers.size()),
        vk_command_buffers
    );
    
    HYPERION_RETURN_OK;
}

Result CommandBuffer::SubmitPrimary(
    VkQueue queue,
    Fence *fence,
    SemaphoreChain *semaphore_chain
)
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
        vkQueueSubmit(queue, 1, &submit_info, fence->GetHandle()),
        "Failed to submit command"
    );

    HYPERION_RETURN_OK;
}

Result CommandBuffer::SubmitSecondary(CommandBuffer *primary)
{
    vkCmdExecuteCommands(
        primary->GetCommandBuffer(),
        1,
        &m_command_buffer
    );

    HYPERION_RETURN_OK;
}

void CommandBuffer::DrawIndexed(
    uint32_t num_indices,
    uint32_t num_instances,
    uint32_t instance_index
) const
{
    vkCmdDrawIndexed(
        m_command_buffer,
        num_indices,
        num_instances,
        0,
        0,
        instance_index
    );
}

void CommandBuffer::BindDescriptorSet(
    const DescriptorPool &pool,
    const GraphicsPipeline *pipeline,
    DescriptorSet::Index set
) const
{
    BindDescriptorSet(
        pool,
        static_cast<const Pipeline *>(pipeline),
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        set,
        set,
        nullptr,
        0
    );
}

void CommandBuffer::BindDescriptorSet(
    const DescriptorPool &pool,
    const GraphicsPipeline *pipeline,
    DescriptorSet::Index set,
    DescriptorSet::Index binding
) const
{
    BindDescriptorSet(
        pool,
        static_cast<const Pipeline *>(pipeline),
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        set,
        binding,
        nullptr,
        0
    );
}

void CommandBuffer::BindDescriptorSet(
    const DescriptorPool &pool,
    const GraphicsPipeline *pipeline,
    DescriptorSet::Index set,
    DescriptorSet::Index binding,
    const uint32_t *offsets,
    size_t num_offsets
) const
{
    BindDescriptorSet(
        pool,
        static_cast<const Pipeline *>(pipeline),
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        set,
        binding,
        offsets,
        num_offsets
    );
}

void CommandBuffer::BindDescriptorSet(
    const DescriptorPool &pool,
    const ComputePipeline *pipeline,
    DescriptorSet::Index set
) const
{
    BindDescriptorSet(
        pool,
        static_cast<const Pipeline *>(pipeline),
        VK_PIPELINE_BIND_POINT_COMPUTE,
        set,
        set,
        nullptr,
        0
    );
}

void CommandBuffer::BindDescriptorSet(
    const DescriptorPool &pool,
    const ComputePipeline *pipeline,
    DescriptorSet::Index set,
    DescriptorSet::Index binding
) const
{
    BindDescriptorSet(
        pool,
        static_cast<const Pipeline *>(pipeline),
        VK_PIPELINE_BIND_POINT_COMPUTE,
        set,
        binding,
        nullptr,
        0
    );
}

void CommandBuffer::BindDescriptorSet(
    const DescriptorPool &pool,
    const ComputePipeline *pipeline,
    DescriptorSet::Index set,
    DescriptorSet::Index binding,
    const uint32_t *offsets,
    size_t num_offsets
) const
{
    BindDescriptorSet(
        pool,
        static_cast<const Pipeline *>(pipeline),
        VK_PIPELINE_BIND_POINT_COMPUTE,
        set,
        binding,
        offsets,
        num_offsets
    );
}

void CommandBuffer::BindDescriptorSet(
    const DescriptorPool &pool,
    const Pipeline *pipeline,
    VkPipelineBindPoint bind_point,
    DescriptorSet::Index set,
    DescriptorSet::Index binding,
    const uint32_t *offsets,
    size_t num_offsets
) const
{
    const auto set_index     = DescriptorSet::GetDesiredIndex(set);
    const auto binding_index = DescriptorSet::GetDesiredIndex(binding);

    const auto &descriptor_sets_view = pool.GetVkDescriptorSets();

    AssertThrowMsg(
        set_index < descriptor_sets_view.size(),
        "Attempt to bind invalid descriptor set (%u) (at index %u) -- out of bounds (max is %llu)\n",
        static_cast<UInt>(set),
        set_index,
        descriptor_sets_view.size()
    );

    const auto &bind_set = descriptor_sets_view[set_index];

    AssertThrowMsg(
        bind_set != nullptr,
        "Attempt to bind invalid descriptor set %u (at index %u) -- set is null\n",
        static_cast<UInt>(set),
        set_index
    );

    vkCmdBindDescriptorSets(
        m_command_buffer,
        bind_point,
        pipeline->layout,
        binding_index,
        1,
        &bind_set,
        static_cast<uint32_t>(num_offsets),
        offsets
    );
}

} // namespace renderer
} // namespace hyperion