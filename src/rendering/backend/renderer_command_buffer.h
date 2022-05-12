#ifndef HYPERION_RENDERER_COMMAND_BUFFER_H
#define HYPERION_RENDERER_COMMAND_BUFFER_H

#include "renderer_device.h"
#include "renderer_render_pass.h"
#include "renderer_semaphore.h"
#include "renderer_fence.h"

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {

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

    inline Type GetType() const { return m_type; }

    inline VkCommandBuffer &GetCommandBuffer() { return m_command_buffer; }
    inline const VkCommandBuffer &GetCommandBuffer() const { return m_command_buffer; }

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

    template <class LambdaFunction>
    inline Result Record(Device *device, const RenderPass *render_pass, const LambdaFunction &fn)
    {
        HYPERION_BUBBLE_ERRORS(Begin(device, render_pass));

        Result result = fn(this);

        HYPERION_PASS_ERRORS(End(device), result);

        return result;
    }

private:
    Type m_type;
    VkCommandBuffer m_command_buffer;
};

} // namespace renderer
} // namespace hyperion

#endif