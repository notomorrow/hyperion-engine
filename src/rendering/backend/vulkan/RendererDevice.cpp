#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererDescriptorSet2.hpp>

#include <cstring>
#include <algorithm>
#include <iterator>

#include <system/Debug.hpp>
#include <util/Defines.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

Device<Platform::VULKAN>::Device(VkPhysicalDevice physical, VkSurfaceKHR surface)
    : m_device(VK_NULL_HANDLE),
      m_physical(physical),
      m_surface(surface),
      m_allocator(VK_NULL_HANDLE),
      m_features(UniquePtr<Features>::Construct()),
      m_descriptor_set_manager(new DescriptorSetManager<Platform::VULKAN>)
{
    m_features->SetPhysicalDevice(m_physical);
    m_queue_family_indices = FindQueueFamilies(m_physical, m_surface);
}

Device<Platform::VULKAN>::~Device()
{
}

void Device<Platform::VULKAN>::SetPhysicalDevice(VkPhysicalDevice physical)
{
    m_physical = physical;
    m_features->SetPhysicalDevice(m_physical);
}

void Device<Platform::VULKAN>::SetRenderSurface(const VkSurfaceKHR &surface)
{
    m_surface = surface;
}

void Device<Platform::VULKAN>::SetRequiredExtensions(const ExtensionMap &extensions)
{
    m_required_extensions = extensions;
}

VkDevice Device<Platform::VULKAN>::GetDevice()
{
    AssertThrow(m_device != VK_NULL_HANDLE);
    return m_device;
}

VkPhysicalDevice Device<Platform::VULKAN>::GetPhysicalDevice()
{
    return m_physical;
}

VkSurfaceKHR Device<Platform::VULKAN>::GetRenderSurface()
{
    AssertThrowMsg(m_surface != VK_NULL_HANDLE, "Surface has not been set!");
    
    return m_surface;
}

QueueFamilyIndices Device<Platform::VULKAN>::FindQueueFamilies(VkPhysicalDevice physical_device, VkSurfaceKHR surface)
{
    QueueFamilyIndices indices{};

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);

    Array<VkQueueFamilyProperties> families;
    families.Resize(queue_family_count);

    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, families.Data());

    constexpr auto possible_flags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT | VK_QUEUE_COMPUTE_BIT;

    /* TODO: move over to QueueFamilyIndices */
    Array<uint32> found_indices;

    const auto predicate = [&](uint32 index, VkQueueFlagBits expected_bits, bool expect_dedicated) -> bool
    {
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
    for (uint32_t i = 0; i < families.Size() && !indices.IsComplete(); i++) {
        AssertContinueMsg(
            families[i].queueCount != 0,
            "Queue family %d supports no queues, skipping\n",
            i
        );

        if (!indices.present_family.HasValue()) {
            VkBool32 supports_presentation = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &supports_presentation);

            if (supports_presentation) {
                DebugLog(LogType::Debug, "Found presentation queue (%d)\n", i);
                indices.present_family = i;
            }
        }

        if (!indices.graphics_family.HasValue()) {
            if (predicate(i, VK_QUEUE_GRAPHICS_BIT, true)) {
                DebugLog(LogType::Debug, "Found dedicated graphics presentation queue (%d)\n", i);
                indices.graphics_family = i;
                found_indices.PushBack(i);
                continue;
            }
        }

        if (!indices.transfer_family.HasValue()) {
            if (predicate(i, VK_QUEUE_TRANSFER_BIT, true)) {
                DebugLog(LogType::Debug, "Found dedicated transfer queue (%d)\n", i);
                indices.transfer_family = i;
                found_indices.PushBack(i);
                continue;
            }
        }

        if (!indices.compute_family.HasValue()) {
            if (predicate(i, VK_QUEUE_COMPUTE_BIT, true)) {
                DebugLog(LogType::Debug, "Found dedicated compute queue (%d)\n", i);
                indices.compute_family = i;
                found_indices.PushBack(i);
                continue;
            }
        }
    }

    AssertThrowMsg(indices.present_family.HasValue(), "No present queue family found!");
    AssertThrowMsg(indices.graphics_family.HasValue(), "No graphics queue family found that supports presentation!");

    if (!indices.transfer_family.HasValue()) {
        DebugLog(LogType::Warn, "No dedicated transfer queue family found!\n");
    }

    if (!indices.compute_family.HasValue()) {
        DebugLog(LogType::Warn, "No dedicated compute queue family found!\n");
    }

    /* Fallback -- find queue families (non-dedicated) */
    for (uint32 i = 0; i < families.Size() && !indices.IsComplete(); i++) {
        AssertContinueMsg(
            families[i].queueCount != 0,
            "Queue family %d supports no queues, skipping\n",
            i
        );

        if (!indices.transfer_family.HasValue()) {
            if (predicate(i, VK_QUEUE_TRANSFER_BIT, false)) {
                DebugLog(LogType::Debug, "Found non-dedicated transfer queue (%d)\n", i);
                indices.transfer_family = i;
            }
        }

        if (!indices.compute_family.HasValue()) {
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
        indices.graphics_family.GetOr(0xBEEF),
        indices.transfer_family.GetOr(0xBEEF),
        indices.present_family.GetOr(0xBEEF),
        indices.compute_family.GetOr(0xBEEF));

    return indices;
}

Array<VkExtensionProperties> Device<Platform::VULKAN>::GetSupportedExtensions()
{
    uint32 extension_count = 0;
    vkEnumerateDeviceExtensionProperties(m_physical, nullptr, &extension_count, nullptr);

    Array<VkExtensionProperties> supported_extensions;
    supported_extensions.Resize(extension_count);

    vkEnumerateDeviceExtensionProperties(m_physical, nullptr, &extension_count, supported_extensions.Data());

    return supported_extensions;
}

ExtensionMap Device<Platform::VULKAN>::GetUnsupportedExtensions()
{
    const Array<VkExtensionProperties> extensions_supported = GetSupportedExtensions();
    ExtensionMap unsupported_extensions;

    for (const auto &required_ext : m_required_extensions) {
        auto supported_it = extensions_supported.FindIf(
            [&required_ext](const auto &it)
            {
                return required_ext.first == it.extensionName;
            }
        );

        if (supported_it == extensions_supported.end()) {
            unsupported_extensions.emplace(required_ext);
        }
    }

    return unsupported_extensions;
}

Result Device<Platform::VULKAN>::CheckDeviceSuitable(const ExtensionMap &unsupported_extensions)
{
    if (!unsupported_extensions.empty()) {
        DebugLog(LogType::Warn, "--- Unsupported Extensions ---\n");
        
        bool any_required = false;

        for (const auto &extension : unsupported_extensions) {
            if (extension.second) {
                DebugLog(LogType::Error, "\t%s [REQUIRED]\n", extension.first.c_str());

                any_required = true;
            } else {
                DebugLog(LogType::Warn, "\t%s\n", extension.first.c_str());
            }
        }

        if (any_required) {
            return { Result::RENDERER_ERR, "Device does not support required extensions" };
        }
    }

    const SwapchainSupportDetails swapchain_support = m_features->QuerySwapchainSupport(m_surface);
    const bool swapchains_available = swapchain_support.formats.Any() && swapchain_support.present_modes.Any();

    if (!m_queue_family_indices.IsComplete()) {
        return {Result::RENDERER_ERR, "Device not supported -- indices setup was not complete."};
    }

    if (!swapchains_available) {
        return {Result::RENDERER_ERR, "Device not supported -- swapchains not available."};
    }

    HYPERION_RETURN_OK;
}

Result Device<Platform::VULKAN>::SetupAllocator(Instance<Platform::VULKAN> *instance)
{
    VmaVulkanFunctions vkfuncs { };
    vkfuncs.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
    vkfuncs.vkGetDeviceProcAddr   = &vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo create_info { };
    create_info.vulkanApiVersion = HYP_VULKAN_API_VERSION;
    create_info.physicalDevice   = m_physical;
    create_info.device           = m_device;
    create_info.instance         = instance->GetInstance();
    create_info.pVulkanFunctions = &vkfuncs;
    create_info.flags            = 0 | (m_features->IsRaytracingSupported() ? VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT : 0);

    vmaCreateAllocator(&create_info, &m_allocator);

    HYPERION_RETURN_OK;
}

void Device<Platform::VULKAN>::DebugLogAllocatorStats() const
{
    if (m_allocator != VK_NULL_HANDLE) {
        char *stats_string;
        vmaBuildStatsString(m_allocator, &stats_string, true);

        DebugLog(LogType::RenInfo, "Pre-destruction VMA stats:\n%s\n", stats_string);

        vmaFreeStatsString(m_allocator, stats_string);
    }
}

Result Device<Platform::VULKAN>::DestroyAllocator()
{
    if (m_allocator != VK_NULL_HANDLE) {
        DebugLogAllocatorStats();

        vmaDestroyAllocator(m_allocator);
        m_allocator = VK_NULL_HANDLE;
    }

    HYPERION_RETURN_OK;
}
Result Device<Platform::VULKAN>::Wait() const
{
    HYPERION_VK_CHECK(vkDeviceWaitIdle(m_device));

    HYPERION_RETURN_OK;
}

Result Device<Platform::VULKAN>::Create(const std::set<uint32> &required_queue_families)
{
    DebugLog(LogType::Debug, "Memory properties:\n");
    const auto &memory_properties = m_features->GetPhysicalDeviceMemoryProperties();

    for (uint32 i = 0; i < memory_properties.memoryTypeCount; i++) {
        const auto &memory_type = memory_properties.memoryTypes[i];
        const uint32 heap_index =  memory_type.heapIndex;

        DebugLog(LogType::Debug, "Memory type %lu:\t(index: %lu, flags: %lu)\n", i, heap_index, memory_type.propertyFlags);

        const VkMemoryHeap &heap = memory_properties.memoryHeaps[heap_index];

        DebugLog(LogType::Debug, "\tHeap:\t\t(size: %llu, flags: %lu)\n", heap.size, heap.flags);
    }

    Array<VkDeviceQueueCreateInfo> queue_create_infos;
    const float priorities[] = { 1.0f };
    // for each queue family(for separate threads) we add them to
    // our device initialization data
    for (uint32 family : required_queue_families) {
        VkDeviceQueueCreateInfo queue_info { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        queue_info.pQueuePriorities = priorities;
        queue_info.queueCount = 1;
        queue_info.queueFamilyIndex = family;

        queue_create_infos.PushBack(queue_info);
    }
    
    const ExtensionMap unsupported_extensions = GetUnsupportedExtensions();
    const auto supported_extensions = GetSupportedExtensions();

    HYPERION_BUBBLE_ERRORS(CheckDeviceSuitable(unsupported_extensions));

    // no _required_ extensions were missing (otherwise would have caused an error)
    // so for each unsupported extension, remove it from out list of extensions
    for (auto &it : unsupported_extensions) {
        AssertThrowMsg(!it.second, "Unsupported extension should not be 'required', should have failed earlier check");

        m_required_extensions.erase(it.first);
    }

    Array<const char *> extension_names;
    extension_names.Reserve(m_required_extensions.size());

    for (const auto &it : m_required_extensions) {
        extension_names.PushBack(it.first.c_str());
    }

    // Vulkan 1.3 requires VK_KHR_portability_subset to be enabled if it is found in vkEnumerateDeviceExtensionProperties()
    // https://vulkan.lunarg.com/doc/view/1.3.211.0/mac/1.3-extensions/vkspec.html#VUID-VkDeviceCreateInfo-pProperties-04451
    {
        auto protability_subset_it = supported_extensions.FindIf(
            [](const auto &it)
            {
                return std::strcmp(it.extensionName, "VK_KHR_portability_subset") == 0;
            }
        );

        if (protability_subset_it != supported_extensions.end()) {
            extension_names.PushBack("VK_KHR_portability_subset");
        }
    }

    DebugLog(LogType::RenDebug, "Required vulkan extensions:\n");
    DebugLog(LogType::RenDebug, "-----\n");

    for (const char *str : extension_names) {
        DebugLog(LogType::RenDebug, "\t%s\n", str);
    }

    DebugLog(LogType::RenDebug, "-----\n");

    VkDeviceCreateInfo create_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    create_info.pQueueCreateInfos       = queue_create_infos.Data();
    create_info.queueCreateInfoCount    = uint32(queue_create_infos.Size());
    // Setup Device extensions
    create_info.enabledExtensionCount   = uint32(extension_names.Size());
    create_info.ppEnabledExtensionNames = extension_names.Data();
    // Setup Device Features
    //create_info.pEnabledFeatures        = &features->GetPhysicalDeviceFeatures();
    create_info.pNext                   = &m_features->GetPhysicalDeviceFeatures2();

    HYPERION_VK_CHECK_MSG(
        vkCreateDevice(m_physical, &create_info, nullptr, &m_device),
        "Could not create Device!"
    );
    
    DebugLog(LogType::Debug, "Loading dynamic functions\n");
    m_features->LoadDynamicFunctions(this);
    m_features->SetDeviceFeatures(this);

    DebugLog(LogType::Info, "Raytracing supported? : %d\n", m_features->IsRaytracingSupported());

    HYPERION_BUBBLE_ERRORS(m_descriptor_set_manager->Create(this));

    HYPERION_RETURN_OK;
}

VkQueue Device<Platform::VULKAN>::GetQueue(uint32 queue_family_index, uint32 queue_index)
{
    AssertThrow(m_device != VK_NULL_HANDLE);

    VkQueue queue;
    vkGetDeviceQueue(m_device, queue_family_index, queue_index, &queue);

    return queue;
}

void Device<Platform::VULKAN>::Destroy()
{
    m_descriptor_set_manager->Destroy(this);

    if (m_device != VK_NULL_HANDLE) {
        /* By the time this destructor is called there should never
         * be a running queue, but just in case we will wait until
         * all the queues on our device are stopped. */
        vkDeviceWaitIdle(m_device);
        vkDestroyDevice(m_device, nullptr);
    }
}

} // namespace platform
} // namespace renderer
} // namespace hyperion
