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
#include "../../system/vma/vma_usage.h"

#include "renderer_device.h"
#include "renderer_swapchain.h"
#include "renderer_shader.h"
#include "renderer_buffer.h"
#include "renderer_graphics_pipeline.h"
#include "renderer_descriptor_pool.h"
#include "renderer_frame.h"
#include "renderer_frame_handler.h"

#define VK_RENDERER_API_VERSION VK_API_VERSION_1_2

namespace hyperion {
namespace renderer {

class Instance {
    static Result CheckValidationLayerSupport(const std::vector<const char *> &requested_layers);

    std::vector<VkPhysicalDevice> EnumeratePhysicalDevices();
    VkPhysicalDevice PickPhysicalDevice(std::vector<VkPhysicalDevice> _devices);

    /* Setup debug mode */
    Result SetupDebug();
    Result SetupDebugMessenger();

    Result AllocatePendingFrames();
    Result SetupAllocator();
    Result DestroyAllocator();

    Result CreateCommandPool();
    Result CreateCommandBuffers();
public:
    Instance(SystemSDL &_system, const char *app_name, const char *engine_name);
    Result Initialize(bool load_debug_layers=false);
    void CreateSurface();
    
    void WaitImageReady(Frame *frame);

    inline DescriptorPool &GetDescriptorPool() { return this->descriptor_pool; }
    inline const DescriptorPool &GetDescriptorPool() const { return this->descriptor_pool; }

    inline VkInstance &GetInstance() { return this->instance; }
    inline VkInstance GetInstance() const { return this->instance; }
    
    void PrepareFrame(Frame *frame);
    void PresentFrame(Frame *frame, SemaphoreChain *semaphore_chain);

    void SetValidationLayers(std::vector<const char *> _layers);

    Device *GetDevice();
    Result InitializeDevice(VkPhysicalDevice _physical_device = nullptr);
    Result InitializeSwapchain();

    inline Swapchain *GetSwapchain() { return swapchain; }
    inline const Swapchain *GetSwapchain() const { return swapchain; }

    void SetQueueFamilies(set<uint32_t> queue_families);
    void SetCurrentWindow(SystemWindow *window);

    inline size_t GetNumImages() const { return this->swapchain->GetNumImages(); }
    inline FrameHandler *GetFrameHandler() { return this->frame_handler; }
    inline const FrameHandler *GetFrameHandler() const { return this->frame_handler; }
    inline VmaAllocator *GetAllocator() { return &this->allocator; }

    SystemWindow *GetCurrentWindow();
    Result Destroy();

    helpers::SingleTimeCommands GetSingleTimeCommands();

    std::vector<const char *> requested_device_extensions;

    const char *app_name;
    const char *engine_name;
    
    Swapchain *swapchain = nullptr;

    /* Per frame data */
    FrameHandler *frame_handler;

    VkCommandPool command_pool;

    VkQueue queue_graphics,
            queue_transfer,
            queue_present,
            queue_compute;

private:

    SystemWindow *window = nullptr;
    SystemSDL    system;

    VkInstance instance = nullptr;
    VkSurfaceKHR surface = nullptr;

    DescriptorPool descriptor_pool;

    VmaAllocator allocator = nullptr;

    Device    *device = nullptr;

    std::set<uint32_t> queue_families;
    std::vector<const char *> validation_layers;

#ifndef HYPERION_BUILD_RELEASE
    VkDebugUtilsMessengerEXT debug_messenger;
#endif
};

} // namespace renderer
} // namespace hyperion

#endif //!RENDERER_INSTANCE
