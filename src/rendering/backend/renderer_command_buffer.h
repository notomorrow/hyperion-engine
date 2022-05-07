#ifndef HYPERION_RENDERER_COMMAND_BUFFER_H
#define HYPERION_RENDERER_COMMAND_BUFFER_H

#include "renderer_device.h"
#include "renderer_render_pass.h"
#include "renderer_semaphore.h"

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {

class CommandBuffer {
public:
    enum Type {
        COMMAND_BUFFER_PRIMARY,
        COMMAND_BUFFER_SECONDARY
    };

    static Result SubmitSecondary(CommandBuffer *primary,
        const std::vector<std::unique_ptr<CommandBuffer>> &command_buffers);
    
    static Result SubmitSecondary(VkCommandBuffer primary,
        VkCommandBuffer *command_buffers,
        size_t num_command_buffers);

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
    Result SubmitPrimary(VkQueue queue,
        VkFence fence,
        SemaphoreChain *semaphore_chain);
    Result SubmitSecondary(VkCommandBuffer primary);
    inline Result SubmitSecondary(CommandBuffer *primary)
        { return SubmitSecondary(primary->GetCommandBuffer()); }
    
    template <class LambdaFunction>
    inline Result RecordCommandsWithContext(const LambdaFunction &fn)
    {
        return fn(this);
    }

    template <class LambdaFunction>
    inline Result Record(Device *device, const RenderPass *render_pass, const LambdaFunction &fn)
    {
        HYPERION_BUBBLE_ERRORS(Begin(device, render_pass));

        Result result = RecordCommandsWithContext<LambdaFunction>(fn);

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