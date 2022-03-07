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
                                       uint32_t queue_index)
{
    vkGetDeviceQueue(device.GetDevice(), queue_family_index, queue_index, &this->queue);
}

VkResult VkRenderer::AcquireNextImage(RendererFrame *frame)
{
    //const VkDevice render_device = this->device->GetDevice();
    AssertExit(frame != nullptr && this->swapchain != nullptr);
    VkDevice render_device = frame->creation_device->GetDevice();

    if (frame->fc_queue_submit != VK_NULL_HANDLE) {
        /* Wait for our queue fence which should have hopefully completed multiple frames
         * earlier. This function should not block at all, but we do this as a precaution. */
        vkWaitForFences(render_device, 1, &frame->fc_queue_submit, true, UINT64_MAX);
        vkResetFences(render_device, 1, &frame->fc_queue_submit);
    }

    VkResult result = vkAcquireNextImageKHR(render_device, this->swapchain->swapchain, UINT64_MAX, frame->sp_swap_acquire, VK_NULL_HANDLE,
                          &this->acquired_frames_index);

    if (result != VK_SUCCESS) return result;

    //if (this->GetCurrentPipeline()->command_pool != VK_NULL_HANDLE)
    //    vkResetCommandPool(render_device, this->GetCurrentPipeline()->command_pool, 0);

    return vkResetCommandBuffer(frame->command_buffer, 0);
}

void VkRenderer::StartFrame(RendererFrame *frame)
{
    AssertThrow(frame != nullptr);

    auto new_image_result = this->AcquireNextImage(frame);

    if (new_image_result == VK_SUBOPTIMAL_KHR || new_image_result == VK_ERROR_OUT_OF_DATE_KHR) {
        DebugLog(LogType::Debug, "Waiting -- image result was %d\n", new_image_result);
        vkDeviceWaitIdle(this->device->GetDevice());
        /* TODO: regenerate framebuffers and swapchain */
    }

    //VkCommandBuffer *cmd = &this->command_buffers[frame_index];
    VkCommandBufferBeginInfo begin_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    //begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    begin_info.pInheritanceInfo = nullptr;
    /* Begin recording our command buffer */
    auto result = vkBeginCommandBuffer(frame->command_buffer, &begin_info);
    AssertThrowMsg(result == VK_SUCCESS, "Failed to start recording command buffer!\n");
}

void VkRenderer::EndFrame(RendererFrame *frame)
{
    auto result = vkEndCommandBuffer(frame->command_buffer);
    AssertThrowMsg(result == VK_SUCCESS, "Failed to record command buffer!\n");

    VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &frame->sp_swap_acquire;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &frame->sp_swap_release;

    submit_info.pWaitDstStageMask = wait_stages;

    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &frame->command_buffer;

    result = vkQueueSubmit(this->queue_graphics, 1, &submit_info, frame->fc_queue_submit);
    AssertThrowMsg(result == VK_SUCCESS, "Failed to submit draw command buffer!\n");
}

void VkRenderer::DrawFrame(RendererFrame *frame)
{
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

RendererResult VkRenderer::CheckValidationLayerSupport(const std::vector<const char *> &requested_layers) {
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
            DebugLog(LogType::Warn, "Validation layer %s is unavailable!\n", request);

            return RendererResult(RendererResult::RENDERER_ERR, "Requested validation layer was unavailable; check debug log for the name of the requested layer");
        }
    }

    HYPERION_RETURN_OK;
}

void VkRenderer::SetValidationLayers(std::vector<const char *> _layers) {
    this->validation_layers = _layers;
}

RendererResult VkRenderer::SetupDebug() {
    static const std::vector<const char *> layers {
        "VK_LAYER_KHRONOS_validation",
        "VK_LAYER_LUNARG_monitor"
    };

    HYPERION_BUBBLE_ERRORS(this->CheckValidationLayerSupport(layers));

    this->SetValidationLayers(layers);
}

void VkRenderer::SetCurrentWindow(SystemWindow *_window) {
    this->window = _window;
}

SystemWindow *VkRenderer::GetCurrentWindow() {
    return this->window;
}


RendererFrame::RendererFrame()
    : creation_device(nullptr),
      command_buffer(nullptr),
      sp_swap_acquire(nullptr),
      sp_swap_release(nullptr),
      fc_queue_submit(nullptr)
{
}

RendererFrame::~RendererFrame()
{
    AssertExitMsg(sp_swap_acquire == nullptr, "sp_swap_acquire should have been destroyed");
    AssertExitMsg(sp_swap_release == nullptr, "sp_swap_release should have been destroyed");
    AssertExitMsg(fc_queue_submit == nullptr, "fc_queue_submit should have been destroyed");
}

RendererResult RendererFrame::Create(non_owning_ptr<RendererDevice> device, VkCommandBuffer cmd)
{
    this->creation_device = device;
    this->command_buffer = cmd;

    return this->CreateSyncObjects();
}

RendererResult RendererFrame::Destroy()
{
    return this->DestroySyncObjects();
}

RendererResult RendererFrame::CreateSyncObjects()
{
    AssertThrow(this->creation_device != nullptr);

    VkDevice rd_device = this->creation_device->GetDevice();
    VkSemaphoreCreateInfo semaphore_info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

    HYPERION_VK_CHECK_MSG(
        vkCreateSemaphore(rd_device, &semaphore_info, nullptr, &this->sp_swap_acquire),
        "Error creating render swap acquire semaphore!"
    );

    HYPERION_VK_CHECK_MSG(
        vkCreateSemaphore(rd_device, &semaphore_info, nullptr, &this->sp_swap_release),
        "Error creating render swap release semaphore!"
    );

    VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    HYPERION_VK_CHECK_MSG(
        vkCreateFence(rd_device, &fence_info, nullptr, &this->fc_queue_submit),
        "Error creating render fence!"
    );

    DebugLog(LogType::Debug, "Create Sync objects!\n");

    HYPERION_RETURN_OK;
}

RendererResult RendererFrame::DestroySyncObjects()
{
    RendererResult result = RendererResult::OK;

    AssertThrow(this->creation_device != nullptr);

    VkDevice rd_device = this->creation_device->GetDevice();

    HYPERION_VK_PASS_ERRORS(vkDeviceWaitIdle(rd_device), result);

    vkDestroySemaphore(rd_device, this->sp_swap_acquire, nullptr);
    this->sp_swap_acquire = nullptr;

    vkDestroySemaphore(rd_device, this->sp_swap_release, nullptr);
    this->sp_swap_release = nullptr;

    vkDestroyFence(rd_device, this->fc_queue_submit, nullptr);
    this->fc_queue_submit = nullptr;

    return result;
}

VkRenderer::VkRenderer(SystemSDL &_system, const char *app_name, const char *engine_name) {
    this->system = _system;
    this->app_name = app_name;
    this->engine_name = engine_name;
    this->device = nullptr;
}

RendererResult VkRenderer::AllocatePendingFrames() {
    AssertExit(this->frames_to_allocate >= 1);
    AssertExitMsg(command_buffers.size() >= this->frames_to_allocate,
                   "Insufficient pipeline command buffers\n");

    this->pending_frames.reserve(this->frames_to_allocate);
    DebugLog(LogType::Debug, "Allocating [%d] frames\n", this->frames_to_allocate);

    for (uint16_t i = 0; i < this->frames_to_allocate; i++) {
        auto frame = std::make_unique<RendererFrame>();

        HYPERION_BUBBLE_ERRORS(frame->Create(non_owning_ptr(this->device), command_buffers[i]));

        this->pending_frames.emplace_back(std::move(frame));
    }

    HYPERION_RETURN_OK;
}

RendererFrame *VkRenderer::GetNextFrame() {
    AssertThrow(this->pending_frames.size());
    ++this->frames_index;
    if (this->frames_index >= this->frames_to_allocate)
        this->frames_index = 0;
    this->current_frame = this->pending_frames[this->frames_index].get();
    //this->current_frame = this->pending_frames[this->frames_index++].get();
    return this->current_frame;
}


RendererResult VkRenderer::CreateCommandPool() {
    QueueFamilyIndices family_indices = this->device->FindQueueFamilies();

    VkCommandPoolCreateInfo pool_info{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    pool_info.queueFamilyIndex = family_indices.graphics_family.value();
    /* TODO: look into VK_COMMAND_POOL_CREATE_TRANSIENT_BIT for constantly changing objects */
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    HYPERION_VK_CHECK_MSG(
        vkCreateCommandPool(this->device->GetDevice(), &pool_info, nullptr, &this->command_pool),
        "Could not create Vulkan command pool"
    );

    DebugLog(LogType::Debug, "Create Command pool\n");

    HYPERION_RETURN_OK;
}

RendererResult VkRenderer::CreateCommandBuffers() {
    this->command_buffers.resize(this->swapchain->images.size());

    VkCommandBufferAllocateInfo alloc_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    alloc_info.commandPool = this->command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = uint32_t(this->command_buffers.size());

    HYPERION_VK_CHECK_MSG(
        vkAllocateCommandBuffers(this->device->GetDevice(), &alloc_info, this->command_buffers.data()),
        "Could not create Vulkan command buffers"
    );

    DebugLog(LogType::Debug, "Allocate %d command buffers\n", this->command_buffers.size());

    HYPERION_RETURN_OK;
}

RendererResult VkRenderer::Initialize(bool load_debug_layers) {
    // Application names/versions
    this->SetCurrentWindow(this->system.GetCurrentWindow());

    /* Set up our debug and validation layers */
    if (load_debug_layers) HYPERION_IGNORE_ERRORS(this->SetupDebug());

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

    HYPERION_VK_CHECK_MSG(
        vkCreateInstance(&create_info, nullptr, &this->instance),
        "Failed to create Vulkan Instance!"
    );

    this->surface = nullptr;
    this->requested_device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    /* Create our renderable surface from SDL */
    this->CreateSurface();
    /* Find and set up an adequate GPU for rendering and presentation */
    HYPERION_BUBBLE_ERRORS(this->InitializeRendererDevice());
    /* Set up our swapchain for our GPU to present our image.
     * This is essentially a "root" framebuffer. */
    HYPERION_BUBBLE_ERRORS(this->InitializeSwapchain());

    /* Create our synchronization objects */
    HYPERION_BUBBLE_ERRORS(CreateCommandPool());
    /* Our command pool will have a command buffer for each frame we can render to. */
    HYPERION_BUBBLE_ERRORS(CreateCommandBuffers());

    HYPERION_BUBBLE_ERRORS(this->AllocatePendingFrames());

    HYPERION_RETURN_OK;
}

RendererResult VkRenderer::CleanupPendingFrames() {
    RendererResult result = RendererResult::OK;

    for (auto &frame : this->pending_frames) {
        HYPERION_PASS_ERRORS(frame->Destroy(), result);

        frame.reset();
    }

    this->pending_frames.clear();

    return result;
}

RendererResult VkRenderer::Destroy() {
    RendererResult result(RendererResult::RENDERER_OK);

    /* Wait for the GPU to finish, we need to be in an idle state. */
    HYPERION_VK_PASS_ERRORS(vkDeviceWaitIdle(this->device->GetDevice()), result);
    /* Cleanup our semaphores and fences! */
    HYPERION_PASS_ERRORS(this->CleanupPendingFrames(), result);

    /* Destroy our pipeline(before everything else!) */
    for (auto &pipeline : this->pipelines) {
        pipeline->Destroy();
        pipeline.reset();
    }

    pipelines.clear();

    vkFreeCommandBuffers(this->device->GetDevice(), this->command_pool, this->command_buffers.size(), this->command_buffers.data());
    vkDestroyCommandPool(this->device->GetDevice(), this->command_pool, nullptr);

    /* Destroy descriptor pool */
    HYPERION_PASS_ERRORS(descriptor_pool.Destroy(this->device), result);

    /* Destroy the vulkan swapchain */
    if (this->swapchain != nullptr)
        HYPERION_PASS_ERRORS(this->swapchain->Destroy(), result);
    delete this->swapchain;
    this->swapchain = nullptr;

    /* Destroy the surface from SDL */
    vkDestroySurfaceKHR(this->instance, this->surface, nullptr);

    /* Destroy our device */
    if (this->device != nullptr)
        this->device->Destroy();
    delete this->device;
    this->device = nullptr;

    /* Destroy the Vulkan instance(this should always be last!) */
    vkDestroyInstance(this->instance, nullptr);
    this->instance = nullptr;

    return result;
}

void VkRenderer::SetQueueFamilies(std::set<uint32_t> _queue_families) {
    this->queue_families = _queue_families;
}

RendererDevice *VkRenderer::GetRendererDevice() {
    return this->device;
}

void VkRenderer::CreateSurface() {
    this->surface = this->GetCurrentWindow()->CreateVulkanSurface(this->instance);
    DebugLog(LogType::Debug, "Created window surface\n");
}

VkPhysicalDevice VkRenderer::PickPhysicalDevice(std::vector<VkPhysicalDevice> _devices)
{
    RendererFeatures::DeviceRequirementsResult device_requirements_result(
        RendererFeatures::DeviceRequirementsResult::DEVICE_REQUIREMENTS_ERR,
        "No device found"
    );

    RendererFeatures device_features;

    /* Check for a discrete/dedicated GPU with geometry shaders */
    for (const auto &_device : _devices) {
        device_features.SetPhysicalDevice(_device);

        if (device_features.IsDiscreteGpu()) {
            if ((device_requirements_result = device_features.SatisfiesMinimumRequirements())) {
                DebugLog(LogType::Info, "Select discrete device %s\n", device_features.GetDeviceName());

                return _device;
            }
        }
    }

    /* No discrete gpu found, look for a device which satisfies requirements */
    for (const auto &_device : _devices) {
        device_features.SetPhysicalDevice(_device);

        if ((device_requirements_result = device_features.SatisfiesMinimumRequirements())) {
            DebugLog(LogType::Info, "Select non-discrete device %s\n", device);

            return _device;
        }
    }

    AssertExit(!_devices.empty());

    auto _device = _devices[0];
    device_features.SetPhysicalDevice(_device);

    device_requirements_result = device_features.SatisfiesMinimumRequirements();

    DebugLog(
        LogType::Error,
        "No device found which satisfied the minimum requirements; selecting device %s.\nThe error message was: %s\n",
        device_features.GetDeviceName(),
        device_requirements_result.message
    );

    /* well shit, we'll just hope for the best at this point */
    return _device;
}

RendererResult VkRenderer::InitializeRendererDevice(VkPhysicalDevice physical_device) {
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
    DebugLog(LogType::Debug, "Found queue family indices\n");

    if (this->queue_families.empty()) {
        DebugLog(LogType::Debug, "queue_families is empty! setting to defaults\n");

        this->SetQueueFamilies({
            family_indices.graphics_family.value(),
            family_indices.present_family.value()
        });
    }

    /* Create a logical device to operate on */
    HYPERION_BUBBLE_ERRORS(
        this->device->CreateLogicalDevice(this->queue_families, this->requested_device_extensions)
    );

    /* Get the internal queues from our device */
    this->queue_graphics = device->GetQueue(family_indices.graphics_family.value(), 0);
    this->queue_present  = device->GetQueue(family_indices.present_family.value(), 0);

    HYPERION_RETURN_OK;
}

RendererResult VkRenderer::AddPipeline(RendererPipeline::Builder &&builder,
    RendererPipeline **out)
{
    HashCode::Value_t hash_code = builder.GetHashCode().Value();

    auto it = std::find_if(this->pipelines.begin(), this->pipelines.end(), [hash_code](const auto &pl) {
        return pl->GetConstructionInfo().GetHashCode().Value() == hash_code;
    });

    if (it != this->pipelines.end()) {
        HYPERION_RETURN_OK;
    }

    auto pipeline = builder.Build(this->device);

    if (out != nullptr) {
        *out = pipeline.get();
    }

    this->pipelines.push_back(std::move(pipeline));

    HYPERION_RETURN_OK;
}

RendererResult VkRenderer::InitializeSwapchain() {
    SwapchainSupportDetails sc_support = this->device->QuerySwapchainSupport();
    QueueFamilyIndices      qf_indices = this->device->FindQueueFamilies();

    this->swapchain = new RendererSwapchain(this->device, sc_support);
    HYPERION_BUBBLE_ERRORS(this->swapchain->Create(this->surface, qf_indices));

    HYPERION_RETURN_OK;
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

helpers::SingleTimeCommands VkRenderer::GetSingleTimeCommands()
{
    QueueFamilyIndices family_indices = this->device->FindQueueFamilies(); // TODO: this result should be cached

    helpers::SingleTimeCommands single_time_commands{};
    single_time_commands.cmd = nullptr;
    single_time_commands.pool = this->command_pool;
    single_time_commands.family_indices = family_indices;

    return single_time_commands;
}

} // namespace hyperion