#ifndef HYPERION_RENDERER_FRAME_H
#define HYPERION_RENDERER_FRAME_H

#include "renderer_result.h"
#include "renderer_device.h"

#include <util/non_owning_ptr.h>

#include <vulkan/vulkan.h>

namespace hyperion {

class RendererFrame {
    RendererResult CreateSyncObjects();
    RendererResult DestroySyncObjects();
public:
    RendererFrame();
    ~RendererFrame();

    RendererResult Create(non_owning_ptr<RendererDevice> device, VkCommandBuffer cmd);
    RendererResult Destroy();

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

    non_owning_ptr<RendererDevice> creation_device;
};

} // namespace hyperion

#endif