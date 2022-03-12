#ifndef HYPERION_RENDERER_COMMAND_BUFFER_H
#define HYPERION_RENDERER_COMMAND_BUFFER_H

#include "renderer_device.h"
#include "renderer_render_pass.h"

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {

class CommandBuffer {
public:

    enum Type {
        COMMAND_BUFFER_PRIMARY,
        COMMAND_BUFFER_SECONDARY
    };

    static Result SubmitSecondary(VkCommandBuffer primary,
        const std::vector<std::unique_ptr<CommandBuffer>> &command_buffers);

    static Result SubmitSecondary(VkCommandBuffer primary,
        VkCommandBuffer *command_buffers,
        size_t num_command_buffers);

    static inline Result SubmitSecondary(CommandBuffer *primary,
        const std::vector<std::unique_ptr<CommandBuffer>> &command_buffers)
        { return SubmitSecondary(primary->GetCommandBuffer(), command_buffers); }

    static inline Result SubmitSecondary(CommandBuffer *primary,
        VkCommandBuffer *command_buffers,
        size_t num_command_buffers)
        { return SubmitSecondary(primary->GetCommandBuffer(), command_buffers, num_command_buffers); }

    static Result SubmitPrimary(VkQueue queue,
        const std::vector<std::unique_ptr<CommandBuffer>> &command_buffers,
        VkFence fence,
        VkSemaphore *wait_semaphores,
        size_t num_wait_semaphores);

    static Result SubmitPrimary(VkQueue queue,
        const std::vector<std::unique_ptr<CommandBuffer>> &command_buffers,
        VkFence fence,
        VkSemaphore *wait_semaphores,
        size_t num_wait_semaphores,
        VkSemaphore *signal_semaphores,
        size_t num_signal_semaphores);

    static Result SubmitPrimary(VkQueue queue,
        VkCommandBuffer *command_buffers,
        size_t num_command_buffers,
        VkFence fence,
        VkSemaphore *wait_semaphores = nullptr,
        size_t num_wait_semaphores = 0,
        VkSemaphore *signal_semaphores = nullptr,
        size_t num_signal_semaphores = 0);

    CommandBuffer(Type type);
    CommandBuffer(const CommandBuffer &other) = delete;
    CommandBuffer &operator=(const CommandBuffer &other) = delete;
    ~CommandBuffer();

    inline VkCommandBuffer &GetCommandBuffer() { return m_command_buffer; }
    inline const VkCommandBuffer &GetCommandBuffer() const { return m_command_buffer; }
    inline VkSemaphore &GetSemaphore() { return m_signal; }
    inline const VkSemaphore &GetSemaphore() const { return m_signal; }

    Result Create(Device *device, VkCommandPool command_pool);
    Result Destroy(Device *device, VkCommandPool command_pool);
    Result Begin(Device *device, const RenderPass *render_pass = nullptr);
    Result End(Device *device);
    Result Reset(Device *device);
    Result SubmitPrimary(VkQueue queue, VkFence fence, VkSemaphore *wait_semaphores = nullptr, size_t num_wait_semaphores = 0);
    Result SubmitSecondary(VkCommandBuffer primary);
    inline Result SubmitSecondary(CommandBuffer *primary)
        { return SubmitSecondary(primary->GetCommandBuffer()); }

    template <class LambdaFunction>
    inline Result Record(Device *device, const RenderPass *render_pass, const LambdaFunction &fn)
    {
        HYPERION_BUBBLE_ERRORS(Begin(device, render_pass));

        Result result = fn(m_command_buffer);

        HYPERION_PASS_ERRORS(End(device), result);

        return result;
    }

private:
    Type m_type;

    VkCommandBuffer m_command_buffer;
    VkSemaphore m_signal;
};

} // namespace renderer
} // namespace hyperion

#endif