//
// Created by ethan on 2/5/22.
//

#include "renderer_instance.h"
#include "renderer_device.h"

#include "../../system/debug.h"

#include <vector>
#include <iostream>
#include <optional>
#include <cstring>

namespace hyperion {
namespace renderer {

static VkResult HandleNextFrame(Device *device, Swapchain *swapchain, Frame *frame, uint32_t *index)
{
    return vkAcquireNextImageKHR(
        device->GetDevice(),
        swapchain->swapchain,
        UINT64_MAX,
        frame->sp_swap_acquire, 
        VK_NULL_HANDLE,
        index);
}

Queue::Queue()
{
    this->queue = nullptr;
}

void Queue::GetQueueFromDevice(Device device, uint32_t queue_family_index,
                                       uint32_t queue_index)
{
    vkGetDeviceQueue(device.GetDevice(), queue_family_index, queue_index, &this->queue);
}

void Instance::WaitImageReady(Frame *frame)
{
    VkResult new_image_result;
    this->frame_handler->AcquireNextImage(device, this->swapchain, &new_image_result);

    if (new_image_result == VK_SUBOPTIMAL_KHR || new_image_result == VK_ERROR_OUT_OF_DATE_KHR) {
        DebugLog(LogType::Debug, "Waiting -- image result was %d\n", new_image_result);
        vkDeviceWaitIdle(this->device->GetDevice());
        /* TODO: regenerate framebuffers and swapchain */
    }
}

void Instance::WaitDeviceIdle()
{
    vkDeviceWaitIdle(this->device->GetDevice());
}

void Instance::BeginFrame(Frame *frame)
{
    /* Assume `frame` is not nullptr */

    this->WaitImageReady(frame);

    frame->BeginCapture();
}

void Instance::EndFrame(Frame *frame)
{
    /* Assume `frame` is not nullptr */

    frame->EndCapture();
}

void Instance::SubmitFrame(Frame *frame)
{
    frame->Submit(this->queue_graphics);
}

void Instance::PresentFrame(Frame *frame, const std::vector<VkSemaphore> &semaphores)
{
    VkPresentInfoKHR present_info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present_info.waitSemaphoreCount = semaphores.size();
    present_info.pWaitSemaphores = semaphores.data();

    AssertThrow(this->swapchain != nullptr || this->swapchain->swapchain != nullptr);

    uint32_t frame_index = this->frame_handler->GetFrameIndex();

    present_info.swapchainCount = 1;
    present_info.pSwapchains = &this->swapchain->swapchain;
    present_info.pImageIndices = &frame_index;
    present_info.pResults = nullptr;
    
    vkQueuePresentKHR(this->queue_present, &present_info);
}

Result Instance::CheckValidationLayerSupport(const std::vector<const char *> &requested_layers)
{
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

            return Result(Result::RENDERER_ERR, "Requested validation layer was unavailable; check debug log for the name of the requested layer");
        }
    }

    HYPERION_RETURN_OK;
}

void Instance::SetValidationLayers(std::vector<const char *> _layers)
{
    this->validation_layers = _layers;
}

#ifndef HYPERION_BUILD_RELEASE

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
        void *user_data)
{
    LogType lt = LogType::Info;
    switch (severity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            lt = LogType::RenDebug;
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            lt = LogType::RenWarn;
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            lt = LogType::RenError;
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            lt = LogType::RenInfo;
            break;
        default:
            break;
    }
    DebugLogRaw(lt, "Vulkan: [%s, %d]:\n\t%s\n",
             callback_data->pMessageIdName, callback_data->messageIdNumber, callback_data->pMessage);
    return VK_FALSE;
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        DebugLog(LogType::Error, "vkCreateDebugUtilsMessengerExt not present! disabling message callback...\n");
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

#endif

Result Instance::SetupDebug()
{
    static const std::vector<const char *> layers {
        "VK_LAYER_KHRONOS_validation",
        "VK_LAYER_LUNARG_monitor"
    };

    HYPERION_BUBBLE_ERRORS(this->CheckValidationLayerSupport(layers));

    this->SetValidationLayers(layers);

    HYPERION_RETURN_OK;
}

void Instance::SetCurrentWindow(SystemWindow *_window) {
    this->window = _window;
}

SystemWindow *Instance::GetCurrentWindow() {
    return this->window;
}

Instance::Instance(SystemSDL &_system, const char *app_name, const char *engine_name)
    : frame_handler(nullptr)
{
    this->swapchain = new Swapchain();
    this->system = _system;
    this->app_name = app_name;
    this->engine_name = engine_name;
    this->device = nullptr;
}

Result Instance::AllocatePendingFrames()
{
    return this->frame_handler->CreateFrames(this->device);
}

Frame *Instance::GetNextFrame()
{
    return this->frame_handler->GetCurrentFrameData().GetFrame();
}

Result Instance::CreateCommandPool()
{
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

Result Instance::CreateCommandBuffers()
{
    return this->frame_handler->CreateCommandBuffers(this->device, this->command_pool);
}


Result Instance::SetupDebugMessenger() {
#ifndef HYPERION_BUILD_RELEASE
    VkDebugUtilsMessengerCreateInfoEXT messenger_info{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    messenger_info.messageSeverity = (
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT    |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT   |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
    );
    messenger_info.messageType = (
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT     |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT  |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
    );
    messenger_info.pfnUserCallback = &DebugCallback;
    messenger_info.pUserData       = nullptr;

    auto result = CreateDebugUtilsMessengerEXT(this->instance, &messenger_info, nullptr, &this->debug_messenger);
    if (result != VK_SUCCESS)
        HYPERION_RETURN_OK;
    DebugLog(LogType::Info, "Using Vulkan Debug Messenger\n");
#endif
    HYPERION_RETURN_OK;
}

Result Instance::Initialize(bool load_debug_layers) {
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

#ifndef HYPERION_BUILD_RELEASE
    extension_names.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

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
    HYPERION_BUBBLE_ERRORS(this->InitializeDevice());
    /* Set up our swapchain for our GPU to present our image.
     * This is essentially a "root" framebuffer. */
    HYPERION_BUBBLE_ERRORS(this->InitializeSwapchain());

    /* Set up our frame handler - this class lets us abstract
     * away a little bit of the double/triple buffering stuff */
    this->frame_handler = new FrameHandler(this->swapchain->GetNumImages(), HandleNextFrame);

    /* Create our synchronization objects */
    HYPERION_BUBBLE_ERRORS(CreateCommandPool());
    /* Our command pool will have a command buffer for each frame we can render to. */
    HYPERION_BUBBLE_ERRORS(CreateCommandBuffers());

    HYPERION_BUBBLE_ERRORS(this->AllocatePendingFrames());

    /* init descriptor sets */
    for (int i = 0; i < DescriptorPool::max_descriptor_sets; i++) {
        this->descriptor_pool.AddDescriptorSet();
    }

    this->SetupDebugMessenger();

    HYPERION_RETURN_OK;
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
    else {
        DebugLog(LogType::Error, "Extension for vkDestroyDebugUtilsMessengerEXT not supported!\n");
    }
}

Result Instance::Destroy() {
    Result result(Result::RENDERER_OK);

    /* Wait for the GPU to finish, we need to be in an idle state. */
    HYPERION_VK_PASS_ERRORS(vkDeviceWaitIdle(this->device->GetDevice()), result);

    /* Destroy our pipeline(before everything else!) */
    for (auto &pipeline : this->pipelines) {
        pipeline->Destroy();
        pipeline.reset();
    }

    pipelines.clear();

    this->frame_handler->Destroy(this->device, this->command_pool);
    delete this->frame_handler;
    this->frame_handler = nullptr;

    vkDestroyCommandPool(this->device->GetDevice(), this->command_pool, nullptr);

    /* Destroy descriptor pool */
    HYPERION_PASS_ERRORS(descriptor_pool.Destroy(this->device), result);

    /* Destroy the vulkan swapchain */
    if (this->swapchain != nullptr) {
        HYPERION_PASS_ERRORS(this->swapchain->Destroy(this->device), result);
        delete this->swapchain;
        this->swapchain = nullptr;
    }

    /* Destroy the surface from SDL */
    vkDestroySurfaceKHR(this->instance, this->surface, nullptr);

    /* Destroy our device */
    if (this->device != nullptr)
        this->device->Destroy();
    delete this->device;
    this->device = nullptr;

#ifndef HYPERION_BUILD_RELEASE
    DestroyDebugUtilsMessengerEXT(this->instance, this->debug_messenger, nullptr);
#endif

    /* Destroy the Vulkan instance(this should always be last!) */
    vkDestroyInstance(this->instance, nullptr);
    this->instance = nullptr;

    return result;
}

void Instance::SetQueueFamilies(std::set<uint32_t> _queue_families) {
    this->queue_families = _queue_families;
}

Device *Instance::GetDevice() {
    return this->device;
}

void Instance::CreateSurface() {
    this->surface = this->GetCurrentWindow()->CreateVulkanSurface(this->instance);
    DebugLog(LogType::Debug, "Created window surface\n");
}

VkPhysicalDevice Instance::PickPhysicalDevice(std::vector<VkPhysicalDevice> _devices)
{
    Features::DeviceRequirementsResult device_requirements_result(
        Features::DeviceRequirementsResult::DEVICE_REQUIREMENTS_ERR,
        "No device found"
    );

    Features device_features;

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

Result Instance::InitializeDevice(VkPhysicalDevice physical_device)
{
    /* If no physical device passed in, we select one */
    if (physical_device == nullptr) {
        std::vector<VkPhysicalDevice> physical_devices = this->EnumeratePhysicalDevices();
        physical_device = this->PickPhysicalDevice(physical_devices);
    }

    if (this->device == nullptr) {
        this->device = new Device();
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

Result Instance::AddPipeline(Pipeline::Builder &&builder,
    Pipeline **out)
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

Result Instance::BuildPipelines()
{
    for (auto &pipeline : this->pipelines) {
        pipeline->Build(&this->descriptor_pool);
    }

    HYPERION_RETURN_OK;
}

Result Instance::InitializeSwapchain()
{
    HYPERION_BUBBLE_ERRORS(this->swapchain->Create(this->device, this->surface));

    HYPERION_RETURN_OK;
}

std::vector<VkPhysicalDevice> Instance::EnumeratePhysicalDevices()
{
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

helpers::SingleTimeCommands Instance::GetSingleTimeCommands()
{
    QueueFamilyIndices family_indices = this->device->FindQueueFamilies(); // TODO: this result should be cached

    helpers::SingleTimeCommands single_time_commands{};
    single_time_commands.cmd = nullptr;
    single_time_commands.pool = this->command_pool;
    single_time_commands.family_indices = family_indices;

    return single_time_commands;
}

} // namespace renderer
} // namespace hyperion