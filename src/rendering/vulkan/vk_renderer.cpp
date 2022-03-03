//
// Created by ethan on 2/5/22.
//

#include "vk_renderer.h"
#include "renderer_device.h"

#include "../../system/debug.h"

#include <vector>
#include <iostream>
#include <optional>
#include <cstring>

namespace hyperion {

RendererQueue::RendererQueue() {
    this->queue = nullptr;
}

void RendererQueue::GetQueueFromDevice(RendererDevice device, uint32_t queue_family_index,
                                       uint32_t queue_index) {
    vkGetDeviceQueue(device.GetDevice(), queue_family_index, queue_index, &this->queue);
}

RendererPipeline *VkRenderer::GetCurrentPipeline() {
    AssertThrowMsg((this->pipeline != nullptr), "Renderer pipeline not initialized!\n");
    return this->pipeline;
}
void VkRenderer::SetCurrentPipeline(RendererPipeline *pipeline) {
    this->pipeline = pipeline;
}

VkResult VkRenderer::AcquireNextImage(RendererFrame *frame) {
    //const VkDevice render_device = this->device->GetDevice();
    AssertThrow(frame != nullptr && this->swapchain != nullptr);
    VkDevice render_device = frame->creation_device->GetDevice();
    if (frame->fc_queue_submit != VK_NULL_HANDLE) {
        /* Wait for our queue fence which should have hopefully completed multiple frames
         * earlier. This function should not block at all, but we do this as a precaution. */
        vkWaitForFences(render_device, 1, &frame->fc_queue_submit, true, UINT64_MAX);
        vkResetFences(render_device, 1, &frame->fc_queue_submit);
    }

    VkResult result = vkAcquireNextImageKHR(render_device, this->swapchain->swapchain, UINT64_MAX, frame->sp_swap_acquire, VK_NULL_HANDLE,
                          &this->acquired_frames_index);
    if (result != VK_SUCCESS)
        return result;

    vkResetCommandBuffer(*(frame->command_buffer), 0);
    //if (this->GetCurrentPipeline()->command_pool != VK_NULL_HANDLE)
    //    vkResetCommandPool(render_device, this->GetCurrentPipeline()->command_pool, 0);

    return VK_SUCCESS;
}

void VkRenderer::StartFrame(RendererFrame *frame) {
    AssertThrow(frame != nullptr);

    auto new_image_result = this->AcquireNextImage(frame);

    if (new_image_result == VK_SUBOPTIMAL_KHR || new_image_result == VK_ERROR_OUT_OF_DATE_KHR) {
        vkDeviceWaitIdle(this->device->GetDevice());
        /* TODO: regenerate framebuffers and swapchain */
    }
    this->pipeline->StartRenderPass(frame->command_buffer, this->acquired_frames_index);
}

void VkRenderer::EndFrame(RendererFrame *frame) {
    this->pipeline->EndRenderPass(frame->command_buffer);

    /* Render objects to the swapchain using our graphics pipeline */
    //this->pipeline->DoRenderPass();

    VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &frame->sp_swap_acquire;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &frame->sp_swap_release;

    submit_info.pWaitDstStageMask = wait_stages;

    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = frame->command_buffer;

    auto result = vkQueueSubmit(this->queue_graphics, 1, &submit_info, frame->fc_queue_submit);

    AssertThrowMsg(result == VK_SUCCESS, "Failed to submit draw command buffer!\n");
}


void VkRenderer::DrawFrame(RendererFrame *frame) {
    VkPresentInfoKHR present_info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &frame->sp_swap_release;

    AssertThrow(this->swapchain != nullptr || this->swapchain->swapchain != nullptr);

    present_info.swapchainCount = 1;
    present_info.pSwapchains = &this->swapchain->swapchain;
    present_info.pImageIndices = &this->acquired_frames_index;
    present_info.pResults = nullptr;

    //vkQueueWaitIdle(this->queue_present);
    vkQueuePresentKHR(this->queue_present, &present_info);
}

bool VkRenderer::CheckValidationLayerSupport(const std::vector<const char *> &requested_layers) {
    uint32_t layers_count;
    vkEnumerateInstanceLayerProperties(&layers_count, nullptr);

    std::vector<VkLayerProperties> available_layers(layers_count);
    vkEnumerateInstanceLayerProperties(&layers_count, available_layers.data());

    for (const char *request : requested_layers) {
        bool layer_found = false;
        for (const auto &available_properties : available_layers) {
            if (!strcmp(available_properties.layerName, request)) {
                layer_found = true;
                break;
            }
        }
        if (!layer_found) {
            // TODO: disable debug mode/validation layers instead of causing a runtime error
            throw std::runtime_error("Validation Layer "+std::string(request)+" is unavailable!");
        }
    }
    return false;
}

void VkRenderer::SetValidationLayers(std::vector<const char *> _layers) {
    this->CheckValidationLayerSupport(_layers);
    this->validation_layers = _layers;
}

void VkRenderer::SetupDebug() {
    const std::vector<const char *> layers = {
        "VK_LAYER_KHRONOS_validation",
    };
    this->SetValidationLayers(layers);
}

void VkRenderer::SetCurrentWindow(SystemWindow *_window) {
    this->window = _window;
}

SystemWindow *VkRenderer::GetCurrentWindow() {
    return this->window;
}

void RendererFrame::Create(RendererDevice *device, VkCommandBuffer *cmd) {
    this->creation_device = device;
    this->command_buffer = cmd;
    this->CreateSyncObjects();
}

void RendererFrame::Destroy() {
    this->DestroySyncObjects();
}

void RendererFrame::CreateSyncObjects() {
    AssertThrow(this->creation_device != nullptr);

    VkDevice rd_device = this->creation_device->GetDevice();
    VkSemaphoreCreateInfo semaphore_info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    auto sa_result = vkCreateSemaphore(rd_device, &semaphore_info, nullptr, &this->sp_swap_acquire);
    auto sr_result = vkCreateSemaphore(rd_device, &semaphore_info, nullptr, &this->sp_swap_release);

    VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    auto fc_result = vkCreateFence(rd_device, &fence_info, nullptr, &this->fc_queue_submit);

    DebugLog(LogType::Debug, "Create Sync objects!\n");

    AssertThrowMsg(fc_result == VK_SUCCESS, "Error creating render fence\n");
    AssertThrowMsg((sa_result == VK_SUCCESS && sr_result == VK_SUCCESS), "Error creating render semaphores!\n");
}

void RendererFrame::DestroySyncObjects() {
    AssertThrow(this->creation_device != nullptr);

    VkDevice rd_device = this->creation_device->GetDevice();
    vkDeviceWaitIdle(rd_device);

    vkDestroySemaphore(rd_device, this->sp_swap_acquire, nullptr);
    vkDestroySemaphore(rd_device, this->sp_swap_release, nullptr);
    vkDestroyFence(rd_device, this->fc_queue_submit, nullptr);
}

VkRenderer::VkRenderer(SystemSDL &_system, const char *app_name, const char *engine_name) {
    this->system = _system;
    this->app_name = app_name;
    this->engine_name = engine_name;
    this->device = nullptr;
}

void VkRenderer::AllocatePendingFrames() {
    AssertThrow(this->frames_to_allocate >= 1);
    AssertThrowMsg(this->pipeline->command_buffers.size() >= this->frames_to_allocate,
                   "Insufficient pipeline command buffers\n");

    this->pending_frames.reserve(this->frames_to_allocate);
    DebugLog(LogType::Debug, "Allocating [%d] frames\n", this->frames_to_allocate);
    for (uint16_t i = 0; i < this->frames_to_allocate; i++) {
        auto *frame = new RendererFrame;
        VkCommandBuffer *cmd = &this->pipeline->command_buffers[i];
        frame->Create(this->device, cmd);
        this->pending_frames.push_back(frame);
    }
}

RendererFrame *VkRenderer::GetNextFrame() {
    AssertThrow(this->pending_frames.size());
    if (this->frames_index >= this->frames_to_allocate)
        this->frames_index = 0;
    this->current_frame = this->pending_frames[this->frames_index++];
    return this->current_frame;
}

void VkRenderer::Initialize(bool load_debug_layers) {
    // Application names/versions
    this->SetCurrentWindow(this->system.GetCurrentWindow());

    /* Set up our debug and validation layers */
    if (load_debug_layers)
        this->SetupDebug();

    VkApplicationInfo app_info{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app_info.pApplicationName = app_name;
    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.pEngineName = engine_name;
    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    // Set target api version
    app_info.apiVersion = VK_RENDERER_API_VERSION;

    VkInstanceCreateInfo create_info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    create_info.pApplicationInfo = &app_info;

    // Setup validation layers
    create_info.enabledLayerCount = (uint32_t)(this->validation_layers.size());
    create_info.ppEnabledLayerNames = this->validation_layers.data();
    // Setup Vulkan extensions
    std::vector<const char *> extension_names;
    extension_names = this->system.GetVulkanExtensionNames();

    create_info.enabledExtensionCount = extension_names.size();
    create_info.ppEnabledExtensionNames = extension_names.data();

    DebugLog(LogType::Info, "Loading [%d] Instance extensions...\n", extension_names.size());

    VkResult result = vkCreateInstance(&create_info, nullptr, &this->instance);
    if (result != VK_SUCCESS) {
        DebugLog(LogType::Fatal, "Failed to create Vulkan Instance!\n");
        throw std::runtime_error("Failed to create Vulkan Instance!");
    }

    this->surface = nullptr;
    this->requested_device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    /* Create our renderable surface from SDL */
    this->CreateSurface();
    /* Find and set up an adequate GPU for rendering and presentation */
    this->InitializeRendererDevice();
    /* Set up our swapchain for our GPU to present our image.
     * This is essentially a "root" framebuffer. */
    this->InitializeSwapchain();
}

void VkRenderer::CleanupPendingFrames() {
    for (auto &frame : this->pending_frames) {
        frame->Destroy();
        delete frame;
    }
}

void VkRenderer::Destroy() {
    /* Wait for the GPU to finish, we need to be in an idle state. */
    vkDeviceWaitIdle(this->device->GetDevice());
    /* Cleanup our semaphores and fences! */
    this->CleanupPendingFrames();

    /* Destroy our pipeline(before everything else!) */
    if (this->pipeline != nullptr)
        this->pipeline->Destroy();
    delete this->pipeline;

    /* Destroy the vulkan swapchain */
    if (this->swapchain != nullptr)
        this->swapchain->Destroy();
    delete this->swapchain;

    /* Destroy the surface from SDL */
    vkDestroySurfaceKHR(this->instance, this->surface, nullptr);

    /* Destroy our device */
    if (this->device != nullptr)
        this->device->Destroy();
    delete this->device;

    /* Destroy the Vulkan instance(this should always be last!) */
    vkDestroyInstance(this->instance, nullptr);
}

void VkRenderer::SetQueueFamilies(std::set<uint32_t> _queue_families) {
    this->queue_families = _queue_families;
}

RendererDevice *VkRenderer::GetRendererDevice() {
    return this->device;
}

void VkRenderer::CreateSurface() {
    this->surface = this->GetCurrentWindow()->CreateVulkanSurface(this->instance);
    std::cout << "Created window surface\n";
}

VkPhysicalDevice VkRenderer::PickPhysicalDevice(std::vector<VkPhysicalDevice> _devices) {
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures   features;

    /* Check for a discrete/dedicated GPU with geometry shaders */
    for (const auto &_device : _devices) {
        vkGetPhysicalDeviceProperties(_device, &properties);
        vkGetPhysicalDeviceFeatures(_device, &features);

        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && features.geometryShader) {
            std::cout << "select device " << properties.deviceName << "\n";
            return _device;
        }
    }

    /* No discrete gpu found, look for a device with at least geometry shaders */
    for (const auto &_device : _devices) {
        vkGetPhysicalDeviceFeatures(_device, &features);
        if (features.geometryShader) {
            return _device;
        }
    }

    AssertExit(!_devices.empty());

    /* well shit, we'll just hope for the best at this point */
    return _devices[0];
}

RendererDevice *VkRenderer::InitializeRendererDevice(VkPhysicalDevice physical_device) {
    /* If no physical device passed in, we select one */
    if (physical_device == nullptr) {
        std::vector<VkPhysicalDevice> physical_devices = this->EnumeratePhysicalDevices();
        physical_device = this->PickPhysicalDevice(physical_devices);
    }

    if (this->device == nullptr) {
        this->device = new RendererDevice();
    }

    this->device->SetRequiredExtensions(this->requested_device_extensions);
    this->device->SetPhysicalDevice(physical_device);
    this->device->SetRenderSurface(this->surface);

    QueueFamilyIndices family_indices = this->device->FindQueueFamilies();

    /* No user specified queue families to create, so we just use the defaults */
    std::cout << "Found queue family indices\n";
    if (this->queue_families.empty()) {
        std::cout << "queue_families is empty! setting to defaults\n";
        this->SetQueueFamilies({
            family_indices.graphics_family.value(),
            family_indices.present_family.value()
        });
    }
    /* Create a logical device to operate on */
    this->device->CreateLogicalDevice(this->queue_families, this->requested_device_extensions);

    /* Get the internal queues from our device */
    this->queue_graphics = device->GetQueue(family_indices.graphics_family.value(), 0);
    this->queue_present  = device->GetQueue(family_indices.present_family.value(), 0);

    return this->device;
}

void VkRenderer::InitializePipeline(RendererShader *render_shader) {
    this->pipeline = new RendererPipeline(this->device, this->swapchain);

    this->pipeline->CreateRenderPass();
    this->swapchain->CreateFramebuffers(this->pipeline->GetRenderPass());
    this->pipeline->Rebuild(render_shader);

    /* Create our synchronization objects */
    this->pipeline->CreateCommandPool();
    /* Our command pool will have a command buffer for each frame we can render to. */
    this->pipeline->CreateCommandBuffers(this->frames_to_allocate);
    DebugLog(LogType::Info, "Created command buffers\n");

    this->AllocatePendingFrames();
}

void VkRenderer::InitializeSwapchain() {
    SwapchainSupportDetails sc_support = this->device->QuerySwapchainSupport();
    QueueFamilyIndices      qf_indices = this->device->FindQueueFamilies();

    this->swapchain = new RendererSwapchain(this->device, sc_support);
    this->swapchain->Create(this->surface, qf_indices);
}

std::vector<VkPhysicalDevice> VkRenderer::EnumeratePhysicalDevices() {
    uint32_t device_count = 0;

    vkEnumeratePhysicalDevices(this->instance, &device_count, nullptr);
    if (device_count == 0) {
        DebugLog(LogType::Fatal, "No devices with Vulkan support found! " \
                                 "Please update your graphics drivers or install a Vulkan compatible device.\n");
        throw std::runtime_error("No GPUs with Vulkan support found!");
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(this->instance, &device_count, devices.data());

    return devices;
}

} // namespace hyperion