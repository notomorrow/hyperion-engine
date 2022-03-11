#ifndef HYPERION_RENDERER_FRAME_H
#define HYPERION_RENDERER_FRAME_H

#include "renderer_result.h"
#include "renderer_device.h"

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

    Result Create(Device *device, VkCommandBuffer cmd);
    Result Destroy();

    /* Start recording into the command buffer */
    void BeginCapture();
    /* Stop recording into the command buffer */
    void EndCapture();
    /* Submit command buffer to the given queue */
    void Submit(VkQueue queue_submit);

    VkCommandBuffer command_buffer;
    /* Sync objects for each frame */
    VkSemaphore sp_swap_acquire;
    VkSemaphore sp_swap_release;
    VkFence     fc_queue_submit;

    non_owning_ptr<Device> creation_device;
};

} // namespace renderer
} // namespace hyperion

#endif