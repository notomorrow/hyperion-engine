#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererSemaphore.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <core/lib/DynArray.hpp>

#include <system/Debug.hpp>
#include <system/Application.hpp>
#include <system/vma/VmaUsage.hpp>
#include <util/Defines.hpp>

#include <Types.hpp>

#include <vector>
#include <optional>
#include <cstring>

namespace hyperion {
namespace renderer {
namespace platform {

static Result HandleNextFrame(
    Device<Platform::VULKAN> *device,
    Swapchain<Platform::VULKAN> *swapchain,
    Frame<Platform::VULKAN> *frame,
    uint32 *index
)
{
    HYPERION_VK_CHECK(vkAcquireNextImageKHR(
        device->GetDevice(),
        swapchain->swapchain,
        UINT64_MAX,
        frame->GetPresentSemaphores().GetWaitSemaphores()[0].Get().GetSemaphore(),
        VK_NULL_HANDLE,
        index
    ));

    HYPERION_RETURN_OK;
}

// Returns supported vulkan debug layers
static Array<const char *> CheckValidationLayerSupport(const Array<const char *> &requested_layers)
{
    Array<const char *> supported_layers;
    supported_layers.Reserve(requested_layers.Size());

    uint32 layers_count;
    vkEnumerateInstanceLayerProperties(&layers_count, nullptr);

    Array<VkLayerProperties> available_layers;
    available_layers.Resize(layers_count);

    vkEnumerateInstanceLayerProperties(&layers_count, available_layers.Data());

    for (const char *request : requested_layers) {
        bool layer_found = false;

        for (const auto &available_properties : available_layers) {
            if (!strcmp(available_properties.layerName, request)) {
                layer_found = true;
                break;
            }
        }

        if (layer_found) {
            supported_layers.PushBack(request);
        } else {
            DebugLog(LogType::Warn, "Validation layer %s is unavailable!\n", request);
        }
    }

    return supported_layers;
}

ExtensionMap Instance<Platform::VULKAN>::GetExtensionMap()
{
    return {
#if HYP_FEATURES_ENABLE_RAYTRACING && HYP_FEATURES_BINDLESS_TEXTURES
        { VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, false },
        { VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, false },
        { VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, false },
        { VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, false },
#endif
        { VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, true },
        { VK_KHR_SPIRV_1_4_EXTENSION_NAME, false },
        { VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME, false },
        { VK_KHR_SWAPCHAIN_EXTENSION_NAME, true },
        { VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME, false }
    };
}

void Instance<Platform::VULKAN>::SetValidationLayers(Array<const char *> validation_layers)
{
    this->validation_layers = std::move(validation_layers);
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

#if HYP_ENABLE_BREAKPOINTS
    if (lt == LogType::RenError) {
        HYP_BREAKPOINT;
    }
#endif
    return VK_FALSE;
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    if (auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"))) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        DebugLog(LogType::Error, "vkCreateDebugUtilsMessengerExt not present! disabling message callback...\n");

        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

#endif

Result Instance<Platform::VULKAN>::SetupDebug()
{
    static const Array<const char *> layers {
        "VK_LAYER_KHRONOS_validation"
#if !defined(HYP_APPLE) || !HYP_APPLE
        , "VK_LAYER_LUNARG_monitor"
#endif
    };

    Array<const char *> supported_layers = CheckValidationLayerSupport(layers);

    SetValidationLayers(std::move(supported_layers));

    HYPERION_RETURN_OK;
}

Instance<Platform::VULKAN>::Instance(RC<Application> application)
    : m_application(application),
      frame_handler(nullptr)
{
    m_swapchain = new Swapchain<Platform::VULKAN>();
}

Result Instance<Platform::VULKAN>::CreateCommandPool(DeviceQueue &queue, uint index)
{
    VkCommandPoolCreateInfo pool_info { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    pool_info.queueFamilyIndex = queue.family;
    /* TODO: look into VK_COMMAND_POOL_CREATE_TRANSIENT_BIT for constantly changing objects */
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    HYPERION_VK_CHECK_MSG(
        vkCreateCommandPool(m_device->GetDevice(), &pool_info, nullptr, &queue.command_pools[index]),
        "Could not create Vulkan command pool"
    );

    DebugLog(LogType::Debug, "Create command pool for queue %d\n", queue.family);

    HYPERION_RETURN_OK;
}

Result Instance<Platform::VULKAN>::SetupDebugMessenger()
{
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
    messenger_info.pUserData = nullptr;

    HYPERION_VK_CHECK(CreateDebugUtilsMessengerEXT(this->instance, &messenger_info, nullptr, &this->debug_messenger));

    DebugLog(LogType::Info, "Using Vulkan Debug Messenger\n");
#endif
    HYPERION_RETURN_OK;
}

Result Instance<Platform::VULKAN>::Initialize(bool load_debug_layers)
{
    /* Set up our debug and validation layers */
    if (load_debug_layers) {
        HYPERION_BUBBLE_ERRORS(SetupDebug());
    }

    VkApplicationInfo app_info { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    app_info.pApplicationName = m_application->GetAppName().Data();
    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.pEngineName = "HyperionEngine";
    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    // Set target api version
    app_info.apiVersion = HYP_VULKAN_API_VERSION;

    VkInstanceCreateInfo create_info { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    create_info.pApplicationInfo = &app_info;
    create_info.enabledLayerCount   = uint32(validation_layers.Size());
    create_info.ppEnabledLayerNames = validation_layers.Data();
    create_info.flags = 0;

#if VK_HEADER_VERSION >= 216
    // for vulkan sdk 1.3.216 and above, enumerate portability extension is required for
    // translation layers such as moltenvk.
    create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    // Setup Vulkan extensions
    Array<const char *> extension_names;

    if (!m_application->GetVkExtensions(extension_names)) {
        return { Result::RENDERER_ERR, "Failed to load Vulkan extensions." };
    }

#ifndef HYPERION_BUILD_RELEASE
    extension_names.PushBack(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

#if VK_HEADER_VERSION >= 216
    // add our enumeration extension to our instance extensions
    extension_names.PushBack(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif

    DebugLog(LogType::Debug, "Got %llu extensions:\n", extension_names.Size());

    for (const char *extension_name : extension_names) {
        DebugLog(LogType::Debug, "\t%s\n", extension_name);
    }

    create_info.enabledExtensionCount = uint32(extension_names.Size());
    create_info.ppEnabledExtensionNames = extension_names.Data();

    DebugLog(LogType::Info, "Loading [%d] Instance extensions...\n", extension_names.Size());

    VkResult instance_result = vkCreateInstance(&create_info, nullptr, &this->instance);

    DebugLog(LogType::Info, "Instance result: %d\n", instance_result);
    HYPERION_VK_CHECK_MSG(
        instance_result,
        "Failed to create Vulkan Instance!"
    );

    this->surface = nullptr;

    /* Create our renderable surface from SDL */
    CreateSurface();
    /* Find and set up an adequate GPU for rendering and presentation */
    HYPERION_BUBBLE_ERRORS(InitializeDevice());
    /* Set up our swapchain for our GPU to present our image.
     * This is essentially a "root" framebuffer. */
    HYPERION_BUBBLE_ERRORS(InitializeSwapchain());

    /* Set up our frame handler - this class lets us abstract
     * away a little bit of the double/triple buffering stuff */
    DebugLog(LogType::RenDebug, "Num swapchain images: %d\n", m_swapchain->NumImages());
    this->frame_handler = new FrameHandler<Platform::VULKAN>(m_swapchain->NumImages(), HandleNextFrame);
    
    /* Our command pool will have a command buffer for each frame we can render to. */
    HYPERION_BUBBLE_ERRORS(this->frame_handler->CreateFrames(m_device, &queue_graphics));

    /* init descriptor sets */

    for (uint i = DescriptorSet::DESCRIPTOR_SET_INDEX_UNUSED; i != DescriptorSet::DESCRIPTOR_SET_INDEX_MAX; i++) {
        const auto index = DescriptorSet::Index(i);
        const auto slot  = DescriptorSet::GetBaseIndex(index);

#if HYP_FEATURES_BINDLESS_TEXTURES
        if (slot == DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES) {
            continue;
        }
#else
        if (slot == DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS) {
            continue;
        }
#endif

        m_device->GetDescriptorPool()->AddDescriptorSet(
            GetDevice(),
            MakeRenderObject<renderer::DescriptorSet, Platform::VULKAN>(
                slot,
                i,
                slot == DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS
            ),
            true // add immediately instead of enqueuing
        );
    }

    //AssertThrow(descriptor_pool.NumDescriptorSets() <= DescriptorSet::max_descriptor_sets);

    SetupDebugMessenger();
    m_device->SetupAllocator(this);

    HYPERION_RETURN_OK;
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    } else {
        DebugLog(LogType::Error, "Extension for vkDestroyDebugUtilsMessengerEXT not supported!\n");
    }
}

Result Instance<Platform::VULKAN>::Destroy()
{
    auto result = Result::OK;

    /* Wait for the GPU to finish, we need to be in an idle state. */
    HYPERION_VK_PASS_ERRORS(vkDeviceWaitIdle(m_device->GetDevice()), result);

    HYPERION_PASS_ERRORS(m_staging_buffer_pool.Destroy(m_device), result);

    this->frame_handler->Destroy(m_device);
    delete this->frame_handler;
    this->frame_handler = nullptr;

    HYPERION_VK_PASS_ERRORS(vkQueueWaitIdle(this->queue_graphics.queue), result);
    HYPERION_VK_PASS_ERRORS(vkQueueWaitIdle(this->queue_compute.queue), result);

    for (auto &command_pool : queue_graphics.command_pools) {
        vkDestroyCommandPool(m_device->GetDevice(), command_pool, nullptr);
    }

    for (auto &command_pool : queue_compute.command_pools) {
        vkDestroyCommandPool(m_device->GetDevice(), command_pool, nullptr);
    }

    m_device->DestroyAllocator();

    /* Destroy the vulkan swapchain */
    if (m_swapchain != nullptr) {
        HYPERION_PASS_ERRORS(m_swapchain->Destroy(m_device), result);
        delete m_swapchain;
        m_swapchain = nullptr;
    }

    /* Destroy the surface from SDL */
    vkDestroySurfaceKHR(this->instance, this->surface, nullptr);

    m_device->Destroy();
    delete m_device;
    m_device = nullptr;

    DebugLog(LogType::Debug, "Destroyed device!\n");

#ifndef HYPERION_BUILD_RELEASE
    DestroyDebugUtilsMessengerEXT(this->instance, this->debug_messenger, nullptr);
#endif
    /* Destroy the Vulkan instance(this should always be last!) */
    vkDestroyInstance(this->instance, nullptr);
    this->instance = nullptr;
    DebugLog(LogType::Debug, "Destroyed instance\n");

    return result;
}

void Instance<Platform::VULKAN>::CreateSurface()
{
    AssertThrow(m_application != nullptr);
    AssertThrow(m_application->GetCurrentWindow() != nullptr);

    surface = m_application->GetCurrentWindow()->CreateVkSurface(this);
    DebugLog(LogType::Debug, "Created window surface\n");
}

VkPhysicalDevice Instance<Platform::VULKAN>::PickPhysicalDevice(std::vector<VkPhysicalDevice> _devices)
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
            DebugLog(LogType::Info, "Select non-discrete device %s\n", device_features.GetDeviceName());

            return _device;
        }
    }

    AssertThrow(!_devices.empty());

    auto _device = _devices[0];
    device_features.SetPhysicalDevice(_device);

    device_requirements_result = device_features.SatisfiesMinimumRequirements();

#if 0
    AssertThrowMsg(
        device_requirements_result,
        "No device found which satisfied the minimum requirements.\nThe error message was: %s\n",
        device_requirements_result.message
    );

#else
    DebugLog(
        LogType::Error,
        "No device found which satisfied the minimum requirements; selecting device %s.\nThe error message was: %s\n",
        device_features.GetDeviceName(),
        device_requirements_result.message
    );
#endif

    /* well shit, we'll just hope for the best at this point */
    return _device;
}

Result Instance<Platform::VULKAN>::InitializeDevice(VkPhysicalDevice physical_device)
{
    /* If no physical device passed in, we select one */
    if (physical_device == nullptr) {
        physical_device = PickPhysicalDevice(EnumeratePhysicalDevices());
    }
    
    m_device = new Device<Platform::VULKAN>(physical_device, this->surface);
    m_device->SetRequiredExtensions(GetExtensionMap());

    const QueueFamilyIndices &family_indices = m_device->GetQueueFamilyIndices();

    /* Put into a set so we don't have any duplicate indices */
    const std::set<uint32> required_queue_family_indices {
        family_indices.graphics_family.Get(),
        family_indices.transfer_family.Get(),
        family_indices.present_family.Get(),
        family_indices.compute_family.Get()
    };
    
    /* Create a logical device to operate on */
    HYPERION_BUBBLE_ERRORS(m_device->Create(required_queue_family_indices));

    /* Get the internal queues from our device */
    this->queue_graphics = {
        .family = family_indices.graphics_family.Get(),
        .queue  = m_device->GetQueue(family_indices.graphics_family.Get())
    };

    this->queue_transfer = {
        .family = family_indices.transfer_family.Get(),
        .queue  = m_device->GetQueue(family_indices.transfer_family.Get())
    };

    this->queue_present = {
        .family = family_indices.present_family.Get(),
        .queue  = m_device->GetQueue(family_indices.present_family.Get())
    };

    this->queue_compute = {
        .family = family_indices.compute_family.Get(),
        .queue  = m_device->GetQueue(family_indices.compute_family.Get())
    };

    for (uint i = 0; i < uint(queue_graphics.command_pools.Size()); i++) {
        HYPERION_BUBBLE_ERRORS(CreateCommandPool(queue_graphics, i));
    }

    for (uint i = 0; i < uint(queue_compute.command_pools.Size()); i++) {
        HYPERION_BUBBLE_ERRORS(CreateCommandPool(queue_compute, i));
    }

    HYPERION_RETURN_OK;
}

Result Instance<Platform::VULKAN>::InitializeSwapchain()
{
    HYPERION_BUBBLE_ERRORS(m_swapchain->Create(m_device, this->surface));

    HYPERION_RETURN_OK;
}

std::vector<VkPhysicalDevice> Instance<Platform::VULKAN>::EnumeratePhysicalDevices()
{
    uint32_t device_count = 0;

    vkEnumeratePhysicalDevices(this->instance, &device_count, nullptr);

    AssertThrowMsg(device_count != 0, "No devices with Vulkan support found! " \
                                      "Please update your graphics drivers or install a Vulkan compatible device.\n");

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(this->instance, &device_count, devices.data());

    return devices;
}

helpers::SingleTimeCommands Instance<Platform::VULKAN>::GetSingleTimeCommands()
{
    const QueueFamilyIndices &family_indices = m_device->GetQueueFamilyIndices();

    helpers::SingleTimeCommands single_time_commands { };
    single_time_commands.pool = this->queue_graphics.command_pools[0];
    single_time_commands.family_indices = family_indices;

    return single_time_commands;
}

} // namespace platform
} // namespace renderer
} // namespace hyperion
