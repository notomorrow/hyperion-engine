//
// Created by emd22 on 2022-02-20.
//

#include "renderer_device.h"
#include "renderer_instance.h"
#include "renderer_features.h"

#include <cstring>
#include <algorithm>

#include "../../system/debug.h"

namespace hyperion {
namespace renderer {
Device::Device(VkPhysicalDevice physical, VkSurfaceKHR surface)
    : device(VK_NULL_HANDLE),
      physical(physical),
      surface(surface),
      allocator(VK_NULL_HANDLE),
      features(new Features())
{
    this->features->SetPhysicalDevice(physical);
    this->queue_family_indices = FindQueueFamilies(this->physical, this->surface);

    static int x = 0;
    DebugLog(LogType::Debug, "Created Device [%d]\n", x++);
}

Device::~Device()
{
    delete features;
}

void Device::SetPhysicalDevice(VkPhysicalDevice physical)
{
    this->physical = physical;
    this->features->SetPhysicalDevice(physical);
}

void Device::SetRenderSurface(const VkSurfaceKHR &surface)
{
    this->surface = surface;
}

void Device::SetRequiredExtensions(const std::vector<const char *> &extensions)
{
    this->required_extensions = extensions;
}

VkDevice Device::GetDevice()
{
    AssertThrow(this->device != nullptr);
    return this->device;
}

VkPhysicalDevice Device::GetPhysicalDevice()
{
    return this->physical;
}

VkSurfaceKHR Device::GetRenderSurface()
{
    if (this->surface == nullptr) {
        DebugLog(LogType::Fatal, "Device render surface is null!\n");
        throw std::runtime_error("Device render surface not set");
    }
    return this->surface;
}

std::vector<const char *> Device::GetRequiredExtensions()
{
    return this->required_extensions;
}

QueueFamilyIndices Device::FindQueueFamilies(VkPhysicalDevice physical_device, VkSurfaceKHR surface)
{
    QueueFamilyIndices indices;

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, families.data());

    constexpr auto possible_flags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT | VK_QUEUE_COMPUTE_BIT;

    /* TODO: move over to QueueFamilyIndices */
    std::vector<uint32_t> found_indices;

    const auto predicate = [&](uint32_t index, VkQueueFlagBits expected_bits, bool expect_dedicated) -> bool {
        const auto masked_bits = families[index].queueFlags & possible_flags;
        
        /* When looking for a dedicate graphics queue, we'll make sure it supports presentation.
         * Some devices appear only to compute and are not graphical,
         * so we need to make sure it supports presenting to the user. */
        if (expected_bits == VK_QUEUE_GRAPHICS_BIT) {
            VkBool32 supports_presentation = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, index, surface, &supports_presentation);

            if (!supports_presentation) {
                return false;
            }
        }

        if (masked_bits & expected_bits) {
            if (!expect_dedicated) {
                return true;
            }

            return std::find(found_indices.begin(), found_indices.end(), index) == found_indices.end();
        }

        return false;
    };

    /* Find dedicated queues */
    for (uint32_t i = 0; i < families.size() && !indices.IsComplete(); i++) {
        AssertContinueMsg(
            families[i].queueCount != 0,
            "Queue family %d supports no queues, skipping\n",
            i
        );

        if (!indices.present_family.has_value()) {
            VkBool32 supports_presentation = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &supports_presentation);

            if (supports_presentation) {
                DebugLog(LogType::Debug, "Found presentation queue (%d)\n", i);
                indices.present_family = i;
            }
        }

        if (!indices.graphics_family.has_value()) {
            if (predicate(i, VK_QUEUE_GRAPHICS_BIT, true)) {
                DebugLog(LogType::Debug, "Found dedicated graphics presentation queue (%d)\n", i);
                indices.graphics_family = i;
                found_indices.push_back(i);
                continue;
            }
        }

        if (!indices.transfer_family.has_value()) {
            if (predicate(i, VK_QUEUE_TRANSFER_BIT, true)) {
                DebugLog(LogType::Debug, "Found dedicated transfer queue (%d)\n", i);
                indices.transfer_family = i;
                found_indices.push_back(i);
                continue;
            }
        }

        if (!indices.compute_family.has_value()) {
            if (predicate(i, VK_QUEUE_COMPUTE_BIT, true)) {
                DebugLog(LogType::Debug, "Found dedicated compute queue (%d)\n", i);
                indices.compute_family = i;
                found_indices.push_back(i);
                continue;
            }
        }
    }

    AssertThrowMsg(indices.present_family.has_value(), "No present queue family found!");
    AssertThrowMsg(indices.graphics_family.has_value(), "No graphics queue family found that supports presentation!");

    if (!indices.transfer_family.has_value()) {
        DebugLog(LogType::Warn, "No dedicated transfer queue family found!\n");
    }

    if (!indices.compute_family.has_value()) {
        DebugLog(LogType::Warn, "No dedicated compute queue family found!\n");
    }

    /* Fallback -- find queue families (non-dedicated) */
    for (uint32_t i = 0; i < families.size() && !indices.IsComplete(); i++) {
        AssertContinueMsg(
            families[i].queueCount != 0,
            "Queue family %d supports no queues, skipping\n",
            i
        );

        if (!indices.transfer_family.has_value()) {
            if (predicate(i, VK_QUEUE_TRANSFER_BIT, false)) {
                DebugLog(LogType::Debug, "Found non-dedicated transfer queue (%d)\n", i);
                indices.transfer_family = i;
            }
        }

        if (!indices.compute_family.has_value()) {
            if (predicate(i, VK_QUEUE_COMPUTE_BIT, false)) {
                DebugLog(LogType::Debug, "Found non-dedicated compute queue (%d)\n", i);
                indices.compute_family = i;
            }
        }
    }

    AssertThrowMsg(indices.IsComplete(), "Queue indices could not be created! Indices were:\n"
        "\tGraphics: %d\n"
        "\tTransfer: %d\n"
        "\tPresent: %d\n",
        "\tCompute: %d\n",
        indices.graphics_family.value_or(0xBEEF),
        indices.transfer_family.value_or(0xBEEF),
        indices.present_family.value_or(0xBEEF),
        indices.compute_family.value_or(0xBEEF));

    return indices;
}

std::vector<VkExtensionProperties> Device::GetSupportedExtensions()
{
    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(this->physical, nullptr, &extension_count, nullptr);
    std::vector<VkExtensionProperties> supported_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(this->physical, nullptr, &extension_count, supported_extensions.data());

    return supported_extensions;
}

std::vector<const char *> Device::CheckExtensionSupport(const std::vector<const char *> &required_extensions)
{
    const auto extensions_supported = this->GetSupportedExtensions();
    std::vector<const char *> unsupported_extensions = required_extensions;

    for (const char *required_ext_name : required_extensions) {
        /* For each extension required, check */
        for (auto &supported : extensions_supported) {
            if (!strcmp(supported.extensionName, required_ext_name)) {
                unsupported_extensions.erase(std::find(unsupported_extensions.begin(), unsupported_extensions.end(), required_ext_name));
            }
        }
    }
    return unsupported_extensions;
}

std::vector<const char *> Device::CheckExtensionSupport()
{
    return this->CheckExtensionSupport(this->GetRequiredExtensions());
}

Result Device::CheckDeviceSuitable()
{
    const std::vector<const char *> unsupported_extensions = this->CheckExtensionSupport();

    if (!unsupported_extensions.empty()) {
        DebugLog(LogType::Warn, "--- Unsupported Extensions ---\n");
        for (const auto &extension : unsupported_extensions) {
            DebugLog(LogType::Warn, "\t%s\n", extension);
        }
        DebugLog(LogType::Error, "Vulkan: Device does not support required extensions\n");
        /* TODO: try different device(s) before exploding  */
        //throw std::runtime_error("Device does not support required extensions");
        return Result(Result::RENDERER_ERR, "Device does not support required extensions");
    }
    bool extensions_available = unsupported_extensions.empty();

    const SwapchainSupportDetails swapchain_support = features->QuerySwapchainSupport(surface);
    bool swapchains_available = (!swapchain_support.formats.empty() && !swapchain_support.present_modes.empty());

    if (!this->queue_family_indices.IsComplete())
        return Result(Result::RENDERER_ERR, "Device not supported -- indices setup was not complete.");

    if (!extensions_available)
        return Result(Result::RENDERER_ERR, "Device not supported -- required extensions were not available.");

    if (!swapchains_available)
        return Result(Result::RENDERER_ERR, "Device not supported -- swapchains not available.");

    HYPERION_RETURN_OK;
}

Result Device::SetupAllocator(Instance *instance)
{
    VmaVulkanFunctions vkfuncs{};
    vkfuncs.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
    vkfuncs.vkGetDeviceProcAddr   = &vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo create_info{};
    create_info.vulkanApiVersion = VK_RENDERER_API_VERSION;
    create_info.physicalDevice   = this->GetPhysicalDevice();
    create_info.device           = this->GetDevice();
    create_info.instance         = instance->GetInstance();
    create_info.pVulkanFunctions = &vkfuncs;

    vmaCreateAllocator(&create_info, &allocator);

    HYPERION_RETURN_OK;
}

Result Device::DestroyAllocator()
{
    if (this->allocator != nullptr) {
        char *stats_string;
        vmaBuildStatsString(this->allocator, &stats_string, true);

        DebugLog(LogType::RenInfo, "Pre-destruction VMA stats:\n%s\n", stats_string);

        vmaFreeStatsString(this->allocator, stats_string);

        vmaDestroyAllocator(this->allocator);
        this->allocator = nullptr;
    }

    HYPERION_RETURN_OK;
}
Result Device::Wait() const
{
    HYPERION_VK_CHECK(vkDeviceWaitIdle(this->device));

    HYPERION_RETURN_OK;
}

Result Device::CreateLogicalDevice(const std::set<uint32_t> &required_queue_families, const std::vector<const char *> &required_extensions)
{
    DebugLog(LogType::Debug, "Memory properties:\n");
    const auto &memory_properties = features->GetPhysicalDeviceMemoryProperties();

    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
        const auto &memory_type = memory_properties.memoryTypes[i];
        const uint32_t heap_index =  memory_type.heapIndex;

        DebugLog(LogType::Debug, "Memory type %lu:\t(index: %lu, flags: %lu)\n", i, heap_index, memory_type.propertyFlags);

        const VkMemoryHeap &heap = memory_properties.memoryHeaps[heap_index];

        DebugLog(LogType::Debug, "\tHeap:\t\t(size: %llu, flags: %lu)\n", heap.size, heap.flags);
    }

    this->SetRequiredExtensions(required_extensions);

    std::vector<VkDeviceQueueCreateInfo> queue_create_info_vec;
    const float priorities[] = { 1.0f };
    // for each queue family(for separate threads) we add them to
    // our device initialization data
    for (uint32_t family : required_queue_families) {
        VkDeviceQueueCreateInfo queue_info{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        queue_info.pQueuePriorities = priorities;
        queue_info.queueCount = 1;
        queue_info.queueFamilyIndex = family;

        queue_create_info_vec.push_back(queue_info);
    }

    HYPERION_BUBBLE_ERRORS(this->CheckDeviceSuitable());

    VkDeviceCreateInfo create_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    create_info.pQueueCreateInfos       = queue_create_info_vec.data();
    create_info.queueCreateInfoCount    = (uint32_t)(queue_create_info_vec.size());
    // Setup Device extensions
    create_info.enabledExtensionCount   = (uint32_t)(required_extensions.size());
    create_info.ppEnabledExtensionNames = required_extensions.data();
    // Setup Device Features
    create_info.pEnabledFeatures        = &features->GetPhysicalDeviceFeatures();
    create_info.pNext                   = &features->GetPhysicalDeviceFeatures2();

    HYPERION_VK_CHECK_MSG(
        vkCreateDevice(this->physical, &create_info, nullptr, &this->device),
        "Could not create Device!"
    );

    HYPERION_RETURN_OK;
}

VkQueue Device::GetQueue(QueueFamilyIndices::Index queue_family_index, uint32_t queue_index)
{
    VkQueue queue;
    vkGetDeviceQueue(this->GetDevice(), queue_family_index, queue_index, &queue);

    return queue;
}

void Device::Destroy()
{
    if (this->device != nullptr) {
        /* By the time this destructor is called there should never
         * be a running queue, but just in case we will wait until
         * all the queues on our device are stopped. */
        vkDeviceWaitIdle(this->device);
        vkDestroyDevice(this->device, nullptr);
    }
}

} // namespace renderer
} // namespace hyperion