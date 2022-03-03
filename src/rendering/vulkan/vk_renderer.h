//
// Created by ethan on 2/5/22.
//

#ifndef HYPERION_VK_RENDERER_H
#define HYPERION_VK_RENDERER_H

#include <vector>
#include <set>
#include <optional>
#include <array>

#include <vulkan/vulkan.h>

#include "../../system/sdl_system.h"

#include "renderer_device.h"
#include "renderer_swapchain.h"
#include "renderer_shader.h"
#include "renderer_buffer.h"
#include "renderer_pipeline.h"

#define VK_RENDERER_API_VERSION VK_API_VERSION_1_2

namespace hyperion {

#define DEFAULT_PENDING_FRAMES_COUNT 2

class RendererFrame {
    void CreateSyncObjects();
    void DestroySyncObjects();
public:
    void Create(RendererDevice *device, VkCommandBuffer *cmd);
    void Destroy();

    VkCommandBuffer *command_buffer;
    /* Sync objects for each frame */
    VkSemaphore sp_swap_acquire;
    VkSemaphore sp_swap_release;
    VkFence     fc_queue_submit;

    RendererDevice *creation_device = nullptr;
};


class RendererQueue {
public:
    RendererQueue();
    void GetQueueFromDevice(RendererDevice device, uint32_t queue_family_index, uint32_t queue_index=0);
private:
    VkQueue queue;
};

class VkRenderer {
    struct DeviceRequirementsResult {
        enum {
            DEVICE_REQUIREMENTS_OK = 0,
            DEVICE_REQUIREMENTS_ERR = 1
        } result;

        const char *message;

        DeviceRequirementsResult(decltype(result) result, const char *message = "")
            : result(result), message(message) {}
        DeviceRequirementsResult(const DeviceRequirementsResult &other)
            : result(other.result), message(other.message) {}
        inline operator bool() const { return result == DEVICE_REQUIREMENTS_OK; }
    };

    static DeviceRequirementsResult DeviceSatisfiesMinimumRequirements(VkPhysicalDeviceFeatures, VkPhysicalDeviceProperties);
    static bool CheckValidationLayerSupport(const std::vector<const char *> &requested_layers);

    std::vector<VkPhysicalDevice> EnumeratePhysicalDevices();
    VkPhysicalDevice PickPhysicalDevice(std::vector<VkPhysicalDevice> _devices,
        VkPhysicalDeviceProperties &out_properties,
        VkPhysicalDeviceFeatures &out_features);

    /* Setup debug mode */
    void SetupDebug();

    void AllocatePendingFrames();
    void CleanupPendingFrames();
public:
    VkRenderer(SystemSDL &_system, const char *app_name, const char *engine_name);
    void Initialize(bool load_debug_layers=false);
    void CreateSurface();

    RendererFrame *GetNextFrame();

    VkResult AcquireNextImage(RendererFrame *frame);
    void     StartFrame(RendererFrame *frame);
    void     EndFrame(RendererFrame *frame);
    void     DrawFrame(RendererFrame *frame);

    void SetValidationLayers(std::vector<const char *> _layers);
    RendererDevice *GetRendererDevice();
    RendererDevice *InitializeRendererDevice(VkPhysicalDevice _physical_device=nullptr);
    void InitializeSwapchain();
    void InitializePipeline(RendererShader *shader);

    void SetQueueFamilies(std::set<uint32_t> queue_families);

    void SetCurrentWindow(SystemWindow *window);
    SystemWindow *GetCurrentWindow();
    RendererPipeline *GetCurrentPipeline();
    void SetCurrentPipeline(RendererPipeline *pipeline);
    void Destroy();

    std::vector<const char *> requested_device_extensions;

    uint16_t frames_to_allocate = DEFAULT_PENDING_FRAMES_COUNT;

    const char *app_name;
    const char *engine_name;
private:
    SystemWindow *window = nullptr;
    SystemSDL    system;

    VkInstance instance = nullptr;
    VkSurfaceKHR surface = nullptr;

    std::vector<RendererFrame *> pending_frames;
    RendererFrame *current_frame = nullptr;
    int frames_index = 0;

    uint32_t acquired_frames_index = 0;

    VkQueue queue_graphics;
    VkQueue queue_present;

    RendererDevice    *device = nullptr;
    RendererSwapchain *swapchain = nullptr;
    RendererPipeline  *pipeline = nullptr;

    std::set<uint32_t> queue_families;
    std::vector<const char *> validation_layers;
};

} // namespace hyperion


#endif //HYPERION_VK_RENDERER_H
