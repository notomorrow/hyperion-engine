//
// Created by emd22 on 2022-02-20.
//

#include "renderer_swapchain.h"

#include "../../system/debug.h"

namespace hyperion {

RendererSwapchain::RendererSwapchain(RendererDevice *_device, const SwapchainSupportDetails &_details)
    : depth_buffer{}
{
    this->swapchain = nullptr;
    this->renderer_device = _device;
    this->support_details = _details;
}

VkSurfaceFormatKHR RendererSwapchain::ChooseSurfaceFormat() {
    for (const auto &format: this->support_details.formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    DebugLog(LogType::Warn, "Swapchain format sRGB is not supported, going with defaults...\n");
    return this->support_details.formats[0];
}

VkPresentModeKHR RendererSwapchain::ChoosePresentMode() {
    /* TODO: add more presentation modes in the future */
    return VK_PRESENT_MODE_IMMEDIATE_KHR;
}

VkExtent2D RendererSwapchain::ChooseSwapchainExtent() {
    return this->support_details.capabilities.currentExtent;
}

void RendererSwapchain::RetrieveImageHandles() {
    uint32_t image_count = 0;
    /* Query for the size, as we will need to create swap chains with more images
     * in the future for more complex applications. */
    vkGetSwapchainImagesKHR(this->renderer_device->GetDevice(), this->swapchain, &image_count, nullptr);
    DebugLog(LogType::Warn, "image count %d\n", image_count);
    this->images.resize(image_count);
    vkGetSwapchainImagesKHR(this->renderer_device->GetDevice(), this->swapchain, &image_count, this->images.data());
    DebugLog(LogType::Info, "Retrieved Swapchain images\n");
}

RendererResult RendererSwapchain::CreateImageViews() {
    this->image_views.resize(this->images.size());

    for (size_t i = 0; i < this->images.size(); i++) {
        VkImage image = this->images[i];

        auto image_view = std::make_unique<RendererImageView>();
        HYPERION_BUBBLE_ERRORS(image_view->Create(
            this->renderer_device,
            image,
            this->image_format
        ));

        this->image_views[i] = std::move(image_view);
    }

    /* create a depth buffer */
    this->depth_buffer.image = std::make_unique<RendererImage>(
        this->extent.width,
        this->extent.height,
        1,
        Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_16,
        Texture::TextureType::TEXTURE_TYPE_2D,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        nullptr
    );

    HYPERION_BUBBLE_ERRORS(this->depth_buffer.image->Create(this->renderer_device, VK_IMAGE_LAYOUT_UNDEFINED));
    this->depth_buffer.image_view = std::make_unique<RendererImageView>();
    HYPERION_BUBBLE_ERRORS(this->depth_buffer.image_view->Create(this->renderer_device, this->depth_buffer.image.get()));

    return RendererResult(RendererResult::RENDERER_OK);
}

void RendererSwapchain::DestroyImageViews() {
    for (auto &image_view : this->image_views) {
        image_view->Destroy(this->renderer_device);
    }

    this->image_views.clear();

    this->depth_buffer.image->Destroy(this->renderer_device);
    this->depth_buffer.image.release();
    this->depth_buffer.image_view->Destroy(this->renderer_device);
    this->depth_buffer.image_view.release();
}

RendererResult RendererSwapchain::CreateFramebuffers(VkRenderPass *renderpass) {
    this->framebuffers.resize(this->image_views.size());
    DebugLog(LogType::Debug, "[%d] image views found!\n", this->image_views.size());

    for (size_t i = 0; i < this->image_views.size(); i++) {
        /*std::vector<VkImageView> attachments;
        attachments.resize(this->image_views.size());

        for (size_t i = 0; i < this->image_views.size(); i++) {
            attachments[i] = this->image_views[i]->GetImageView();
        }*/

        const std::array attachments{
            this->image_views[i]->GetImageView(),
            this->depth_buffer.image_view->GetImageView()
        };

        VkFramebufferCreateInfo create_info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        create_info.renderPass = *renderpass;
        create_info.attachmentCount = attachments.size();
        create_info.pAttachments = attachments.data();
        create_info.width  = this->extent.width;
        create_info.height = this->extent.height;
        create_info.layers = 1;

        HYPERION_VK_CHECK_MSG(
            vkCreateFramebuffer(this->renderer_device->GetDevice(), &create_info, nullptr, &this->framebuffers[i]),
            "Could not create Vulkan framebuffer!"
        );

        DebugLog(LogType::Debug, "Created Pipeline Framebuffer #%d\n", i);
    }

    return RendererResult(RendererResult::RENDERER_OK);
}

RendererResult RendererSwapchain::Create(const VkSurfaceKHR &surface, QueueFamilyIndices qf_indices) {
    this->surface_format = this->ChooseSurfaceFormat();
    this->present_mode = this->ChoosePresentMode();
    this->extent = this->ChooseSwapchainExtent();
    this->image_format = this->surface_format.format;

    uint32_t image_count = this->support_details.capabilities.minImageCount + 1;
    DebugLog(LogType::Debug, "Min images required: %d\n", this->support_details.capabilities.minImageCount);

    VkSwapchainCreateInfoKHR create_info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    create_info.surface = surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1; /* This is always 1 unless we make a stereoscopic/VR application */
    create_info.imageUsage = this->image_usage_flags;

    /* Graphics computations and presentation are done on separate hardware */
    if (qf_indices.graphics_family != qf_indices.present_family) {
        DebugLog(LogType::Info, "Swapchain sharing mode set to Concurrent!\n");
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2; /* Two family indices(one for each process) */
        const uint32_t families[] = {qf_indices.graphics_family.value(), qf_indices.present_family.value()};
        create_info.pQueueFamilyIndices = families;
    }
        /* Computations and presentation are done on same hardware(most scenarios) */
    else {
        DebugLog(LogType::Info, "Swapchain sharing mode set to Exclusive!\n");
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
        vkCreateSwapchainKHR(this->renderer_device->GetDevice(), &create_info, nullptr, &this->swapchain),
        "Failed to create Vulkan swapchain!"
    );

    DebugLog(LogType::Debug, "Created Swapchain!\n");

    this->RetrieveImageHandles();

    HYPERION_BUBBLE_ERRORS(this->CreateImageViews());

    return RendererResult(RendererResult::RENDERER_OK);
}

void RendererSwapchain::Destroy() {
    for (auto framebuffer: this->framebuffers) {
        vkDestroyFramebuffer(this->renderer_device->GetDevice(), framebuffer, nullptr);
    }
    this->DestroyImageViews();
    vkDestroySwapchainKHR(this->renderer_device->GetDevice(), this->swapchain, nullptr);
    DebugLog(LogType::Info, "Destroying swapchain\n");
}

};