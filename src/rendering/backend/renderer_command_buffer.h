#ifndef HYPERION_RENDERER_COMMAND_BUFFER_H
#define HYPERION_RENDERER_COMMAND_BUFFER_H

#include "renderer_device.h"

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {

class CommandBuffer {
public:
    static Result Submit(VkQueue queue,
        const std::vector<std::unique_ptr<CommandBuffer>> &command_buffers,
        VkFence fence,
        VkSemaphore *wait_semaphores,
        size_t num_wait_semaphores);

    static Result Submit(VkQueue queue,
        const std::vector<std::unique_ptr<CommandBuffer>> &command_buffers,
        VkFence fence,
        VkSemaphore *wait_semaphores,
        size_t num_wait_semaphores,
        VkSemaphore *signal_semaphores,
        size_t num_signal_semaphores);

    static Result Submit(VkQueue queue,
        VkCommandBuffer *command_buffers,
        size_t num_command_buffers,
        VkFence fence,
        VkSemaphore *wait_semaphores = nullptr,
        size_t num_wait_semaphores = 0,
        VkSemaphore *signal_semaphores = nullptr,
        size_t num_signal_semaphores = 0);

    CommandBuffer();
    CommandBuffer(const CommandBuffer &other) = delete;
    CommandBuffer &operator=(const CommandBuffer &other) = delete;
    ~CommandBuffer();

    inline VkCommandBuffer GetCommandBuffer() const { return m_command_buffer; }
    inline VkSemaphore GetSemaphore() const { return m_signal; }

    Result Create(Device *device, VkCommandPool command_pool);
    Result Destroy(Device *device, VkCommandPool command_pool);
    Result Begin(Device *device);
    Result End(Device *device);
    Result Submit(VkQueue queue, VkFence fence, VkSemaphore *wait_semaphores = nullptr, size_t num_wait_semaphores = 0);

    template <class LambdaFunction>
    inline Result Record(Device *device, const LambdaFunction &fn)
    {
        HYPERION_BUBBLE_ERRORS(Begin(device));

        Result result = fn(m_command_buffer);

        HYPERION_PASS_ERRORS(End(device), result);

        return result;
    }

private:
    VkCommandBuffer m_command_buffer;
    VkSemaphore m_signal;
};

} // namespace renderer
} // namespace hyperion

#endif