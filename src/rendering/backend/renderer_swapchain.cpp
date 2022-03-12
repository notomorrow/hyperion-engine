//
// Created by emd22 on 2022-02-20.
//

#include "renderer_swapchain.h"

#include "../../system/debug.h"

namespace hyperion {
namespace renderer {
Swapchain::Swapchain()
{
    this->swapchain = nullptr;
}

VkSurfaceFormatKHR Swapchain::ChooseSurfaceFormat()
{
    for (const auto &format : this->support_details.formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    DebugLog(LogType::Warn, "Swapchain format sRGB is not supported, going with defaults...\n");
    return this->support_details.formats[0];
}

VkPresentModeKHR Swapchain::GetPresentMode()
{
    return VK_PRESENT_MODE_IMMEDIATE_KHR;
}

VkExtent2D Swapchain::ChooseSwapchainExtent()
{
    return this->support_details.capabilities.currentExtent;
}

void Swapchain::RetrieveImageHandles(Device *device)
{
    uint32_t image_count = 0;
    /* Query for the size, as we will need to create swap chains with more images
     * in the future for more complex applications. */
    vkGetSwapchainImagesKHR(device->GetDevice(), this->swapchain, &image_count, nullptr);
    DebugLog(LogType::Warn, "image count %d\n", image_count);
    this->images.resize(image_count);
    vkGetSwapchainImagesKHR(device->GetDevice(), this->swapchain, &image_count, this->images.data());
    DebugLog(LogType::Info, "Retrieved Swapchain images\n");
}

void Swapchain::RetrieveSupportDetails(Device *device)
{
    this->support_details = device->QuerySwapchainSupport();
}


Result Swapchain::Create(Device *device, const VkSurfaceKHR &surface)
{
    this->RetrieveSupportDetails(device);

    this->surface_format = this->ChooseSurfaceFormat();
    this->present_mode = this->GetPresentMode();
    this->extent = this->ChooseSwapchainExtent();
    this->image_format = this->surface_format.format;

    uint32_t image_count = this->support_details.capabilities.minImageCount + 1; // is this +1 going to cause problems?
    DebugLog(LogType::Debug, "Min images required: %d\n", this->support_details.capabilities.minImageCount);

    VkSwapchainCreateInfoKHR create_info{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    create_info.surface = surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1; /* This is always 1 unless we make a stereoscopic/VR application */
    create_info.imageUsage = this->image_usage_flags;

    /* Graphics computations and presentation are done on separate hardware */
    QueueFamilyIndices qf_indices = device->FindQueueFamilies();

    if (qf_indices.graphics_family != qf_indices.present_family) {
        DebugLog(LogType::Debug, "Swapchain sharing mode set to Concurrent\n");
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2; /* Two family indices(one for each process) */
        const uint32_t families[] = { qf_indices.graphics_family.value(), qf_indices.present_family.value() };
        create_info.pQueueFamilyIndices = families;
    } else {
        /* Computations and presentation are done on same hardware(most scenarios) */
        DebugLog(LogType::Debug, "Swapchain sharing mode set to Exclusive\n");
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0;       /* Optional */
        create_info.pQueueFamilyIndices = nullptr; /* Optional */
    }
    /* For transformations such as rotations, etc. */
    create_info.preTransform = this->support_details.capabilities.currentTransform;
    /* This can be used to blend with other windows in the windowing system, but we
     * simply leave it opaque.*/
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    HYPERION_VK_CHECK_MSG(
        vkCreateSwapchainKHR(device->GetDevice(), &create_info, nullptr, &this->swapchain),
        "Failed to create Vulkan swapchain!"
    );

    DebugLog(LogType::Debug, "Created Swapchain!\n");

    this->RetrieveImageHandles(device);

    HYPERION_RETURN_OK;
}

Result Swapchain::Destroy(Device *device)
{
    DebugLog(LogType::Debug, "Destroying swapchain\n");

    Result result(Result::RENDERER_OK);

    vkDestroySwapchainKHR(device->GetDevice(), this->swapchain, nullptr);

    return result;
}

} // namespace renderer
} // namespace hyperion