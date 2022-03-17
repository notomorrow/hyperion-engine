#ifndef HYPERION_RENDERER_FRAME_H
#define HYPERION_RENDERER_FRAME_H

#include "renderer_result.h"
#include "renderer_device.h"
#include "renderer_command_buffer.h"
#include "renderer_semaphore.h"

#include <util/non_owning_ptr.h>

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {

using ::hyperion::non_owning_ptr;

class Frame {
    Result CreateSyncObjects();
    Result DestroySyncObjects();
public:
    Frame();
    Frame(const Frame &other) = delete;
    Frame &operator=(const Frame &other) = delete;
    ~Frame();

    Result Create(Device *device, const non_owning_ptr<CommandBuffer> &cmd);
    Result Destroy();

    inline CommandBuffer *GetCommandBuffer() { return command_buffer.get(); }
    inline const CommandBuffer *GetCommandBuffer() const { return command_buffer.get(); }

    /* Start recording into the command buffer */
    void BeginCapture();
    /* Stop recording into the command buffer */
    void EndCapture();
    /* Submit command buffer to the given queue */
    void Submit(VkQueue queue_submit, SemaphoreChain *semaphore_chain);
    
    non_owning_ptr<CommandBuffer> command_buffer;
    VkFence     fc_queue_submit;

    non_owning_ptr<Device> creation_device;
};

} // namespace renderer
} // namespace hyperion

#endif