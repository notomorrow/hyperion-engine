//
// Created by ethan on 2/5/22.
//

#ifndef HYPERION_RENDERER_INSTANCE_H
#define HYPERION_RENDERER_INSTANCE_H

#include <vector>
#include <set>
#include <optional>
#include <array>

#include <vulkan/vulkan.h>

#include <util/non_owning_ptr.h>
#include "../../system/sdl_system.h"

#include "renderer_device.h"
#include "renderer_swapchain.h"
#include "renderer_shader.h"
#include "renderer_buffer.h"
#include "renderer_graphics_pipeline.h"
#include "renderer_descriptor_pool.h"
#include "renderer_frame.h"
#include "renderer_frame_handler.h"

#define VK_RENDERER_API_VERSION VK_API_VERSION_1_2

/* Max frames/sync objects to have available to render to. This prevents the graphics
 * pipeline from stalling when waiting for device upload/download.
 * It's possible a device will return 0 for maxImageCount which indicates
 * there is no limit. So we cap it ourselves.
 */

#define NUM_MAX_PENDING_FRAMES uint32_t(8)

namespace hyperion {
namespace renderer {

using ::std::vector,
      ::std::set;

class Queue {
public:
    Queue();
    void GetQueueFromDevice(Device device, uint32_t queue_family_index, uint32_t queue_index=0);
private:
    VkQueue queue;
};

class Instance {
    static Result CheckValidationLayerSupport(const vector<const char *> &requested_layers);

    vector<VkPhysicalDevice> EnumeratePhysicalDevices();
    VkPhysicalDevice PickPhysicalDevice(vector<VkPhysicalDevice> _devices);

    /* Setup debug mode */
    Result SetupDebug();
    Result SetupDebugMessenger();

    Result AllocatePendingFrames();

    Result CreateCommandPool();
    Result CreateCommandBuffers();
public:
    Instance(SystemSDL &_system, const char *app_name, const char *engine_name);
    Result Initialize(bool load_debug_layers=false);
    void CreateSurface();

    Frame *GetNextFrame();
    void WaitImageReady(Frame *frame);
    void WaitDeviceIdle();

    HYP_FORCE_INLINE DescriptorPool &GetDescriptorPool() { return this->descriptor_pool; }
    HYP_FORCE_INLINE const DescriptorPool &GetDescriptorPool() const { return this->descriptor_pool; }
    
    void     BeginFrame      (Frame *frame);
    void     EndFrame        (Frame *frame);
    void     SubmitFrame     (Frame *frame);
    void     PresentFrame    (Frame *frame, const std::vector<VkSemaphore> &semaphores);

    void SetValidationLayers(vector<const char *> _layers);
    Device *GetDevice();
    Result InitializeDevice(VkPhysicalDevice _physical_device = nullptr);
    Result InitializeSwapchain();

    void SetQueueFamilies(set<uint32_t> queue_families);
    void SetCurrentWindow(SystemWindow *window);

    inline size_t GetNumImages() const { return this->swapchain->GetNumImages(); }
    inline FrameHandler *GetFrameHandler() { return this->frame_handler; }
    inline const FrameHandler *GetFrameHandler() const { return this->frame_handler; }

    SystemWindow *GetCurrentWindow();
    Result Destroy();

    helpers::SingleTimeCommands GetSingleTimeCommands();

    vector<const char *> requested_device_extensions;

    const char *app_name;
    const char *engine_name;
    
    Swapchain *swapchain = nullptr;

    /* Per frame data */
    FrameHandler *frame_handler;

    VkCommandPool command_pool;

    VkQueue queue_graphics;
    VkQueue queue_present;


private:

    SystemWindow *window = nullptr;
    SystemSDL    system;

    VkInstance instance = nullptr;
    VkSurfaceKHR surface = nullptr;

    DescriptorPool descriptor_pool;
    

    Device    *device = nullptr;

    set<uint32_t> queue_families;
    vector<const char *> validation_layers;

#ifndef HYPERION_BUILD_RELEASE
    VkDebugUtilsMessengerEXT debug_messenger;
#endif
};

} // namespace renderer
} // namespace hyperion

#endif //!RENDERER_INSTANCE
