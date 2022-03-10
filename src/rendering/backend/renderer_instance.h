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
#include "renderer_pipeline.h"
#include "renderer_descriptor_pool.h"
#include "renderer_frame.h"

#define VK_RENDERER_API_VERSION VK_API_VERSION_1_2

namespace hyperion {
namespace renderer {
#define DEFAULT_PENDING_FRAMES_COUNT 2

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
    Result CleanupPendingFrames();

    Result CreateCommandPool();
    Result CreateCommandBuffers();
public:
    Instance(SystemSDL &_system, const char *app_name, const char *engine_name);
    Result Initialize(bool load_debug_layers=false);
    void CreateSurface();

    Frame *GetNextFrame();
    void WaitImageReady(Frame *frame);
    void WaitDeviceIdle();

    HYP_FORCE_INLINE Frame *GetCurrentFrame() { return this->current_frame; }
    HYP_FORCE_INLINE const Frame *GetCurrentFrame() const { return this->current_frame; }

    HYP_FORCE_INLINE DescriptorPool &GetDescriptorPool() { return this->descriptor_pool; }
    HYP_FORCE_INLINE const DescriptorPool &GetDescriptorPool() const { return this->descriptor_pool; }

    VkResult AcquireNextImage(Frame *frame);
    void     BeginFrame      (Frame *frame);
    void     EndFrame        (Frame *frame);
    void     PresentFrame    (Frame *frame);

    void SetValidationLayers(vector<const char *> _layers);
    Device *GetDevice();
    Result InitializeDevice(VkPhysicalDevice _physical_device = nullptr);
    Result InitializeSwapchain();

    Result AddPipeline(Pipeline::Builder &&builder, Pipeline **out = nullptr);
    Result BuildPipelines();

    void SetQueueFamilies(set<uint32_t> queue_families);
    void SetCurrentWindow(SystemWindow *window);

    SystemWindow *GetCurrentWindow();
    Result Destroy();

    helpers::SingleTimeCommands GetSingleTimeCommands();

    vector<const char *> requested_device_extensions;

    uint16_t frames_to_allocate = DEFAULT_PENDING_FRAMES_COUNT;

    const char *app_name;
    const char *engine_name;

    vector<std::unique_ptr<Pipeline>> pipelines;

    uint32_t acquired_frames_index = 0;
    Swapchain *swapchain = nullptr;

    /* Per frame data */
    VkCommandPool command_pool;
    vector<VkCommandBuffer> command_buffers;

private:
    SystemWindow *window = nullptr;
    SystemSDL    system;

    VkInstance instance = nullptr;
    VkSurfaceKHR surface = nullptr;

    DescriptorPool descriptor_pool;

    vector<std::unique_ptr<Frame>> pending_frames;
    Frame *current_frame = nullptr;
    int frames_index = 0;

    VkQueue queue_graphics;
    VkQueue queue_present;

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
