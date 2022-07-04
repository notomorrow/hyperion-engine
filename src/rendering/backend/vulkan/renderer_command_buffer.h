#ifndef HYPERION_RENDERER_COMMAND_BUFFER_H
#define HYPERION_RENDERER_COMMAND_BUFFER_H

#include "renderer_device.h"
#include "renderer_render_pass.h"
#include "renderer_semaphore.h"
#include "renderer_fence.h"
#include "renderer_descriptor_set.h"

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {

class Pipeline;
class GraphicsPipeline;
class ComputePipeline;
class RaytracingPipeline;

class CommandBuffer {
public:
    enum Type {
        COMMAND_BUFFER_PRIMARY,
        COMMAND_BUFFER_SECONDARY
    };

    static Result SubmitSecondary(
        CommandBuffer *primary,
        const std::vector<std::unique_ptr<CommandBuffer>> &command_buffers
    );

    CommandBuffer(Type type);
    CommandBuffer(const CommandBuffer &other) = delete;
    CommandBuffer &operator=(const CommandBuffer &other) = delete;
    ~CommandBuffer();

    Type GetType() const { return m_type; }

    VkCommandBuffer GetCommandBuffer() const { return m_command_buffer; }

    Result Create(Device *device, VkCommandPool command_pool);
    Result Destroy(Device *device, VkCommandPool command_pool);
    Result Begin(Device *device, const RenderPass *render_pass = nullptr);
    Result End(Device *device);
    Result Reset(Device *device);
    Result SubmitPrimary(
        VkQueue queue,
        Fence *fence,
        SemaphoreChain *semaphore_chain
    );

    Result SubmitSecondary(CommandBuffer *primary);

    void DrawIndexed(
        uint32_t num_indices,
        uint32_t num_instances  = 1,
        uint32_t instance_index = 0
    ) const;

    void BindDescriptorSet(
        const DescriptorPool &pool,
        const GraphicsPipeline *pipeline,
        DescriptorSet::Index set
    ) const;

    void BindDescriptorSet(
        const DescriptorPool &pool,
        const GraphicsPipeline *pipeline,
        DescriptorSet::Index set,
        DescriptorSet::Index binding
    ) const;

    void BindDescriptorSet(
        const DescriptorPool &pool,
        const GraphicsPipeline *pipeline,
        DescriptorSet::Index set,
        DescriptorSet::Index binding,
        const uint32_t *offsets,
        size_t num_offsets
    ) const;

    template <size_t Size>
    void BindDescriptorSet(
        const DescriptorPool &pool,
        const GraphicsPipeline *pipeline,
        DescriptorSet::Index set,
        DescriptorSet::Index binding,
        const std::array<uint32_t, Size> &offsets
    ) const
    {
        BindDescriptorSet(
            pool,
            pipeline,
            set,
            binding,
            offsets.data(),
            offsets.size()
        );
    }

    void BindDescriptorSet(
        const DescriptorPool &pool,
        const ComputePipeline *pipeline,
        DescriptorSet::Index set
    ) const;

    void BindDescriptorSet(
        const DescriptorPool &pool,
        const ComputePipeline *pipeline,
        DescriptorSet::Index set,
        DescriptorSet::Index binding
    ) const;

    void BindDescriptorSet(
        const DescriptorPool &pool,
        const ComputePipeline *pipeline,
        DescriptorSet::Index set,
        DescriptorSet::Index binding,
        const uint32_t *offsets,
        size_t num_offsets
    ) const;

    template <size_t Size>
    void BindDescriptorSet(
        const DescriptorPool &pool,
        const ComputePipeline *pipeline,
        DescriptorSet::Index set,
        DescriptorSet::Index binding,
        const std::array<uint32_t, Size> &offsets
    ) const
    {
        BindDescriptorSet(
            pool,
            pipeline,
            set,
            binding,
            offsets.data(),
            offsets.size()
        );
    }

    template <class LambdaFunction>
    Result Record(Device *device, const RenderPass *render_pass, const LambdaFunction &fn)
    {
        HYPERION_BUBBLE_ERRORS(Begin(device, render_pass));

        Result result = fn(this);

        HYPERION_PASS_ERRORS(End(device), result);

        return result;
    }

private:
    void BindDescriptorSet(
        const DescriptorPool &pool,
        const Pipeline *pipeline,
        VkPipelineBindPoint bind_point,
        DescriptorSet::Index set,
        DescriptorSet::Index binding,
        const uint32_t *offsets,
        size_t num_offsets
    ) const;

    Type m_type;
    VkCommandBuffer m_command_buffer;
};

} // namespace renderer
} // namespace hyperion

#endif