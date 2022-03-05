//
// Created by emd22 on 2022-02-20.
//

#ifndef HYPERION_RENDERER_SWAPCHAIN_H
#define HYPERION_RENDERER_SWAPCHAIN_H

#include "renderer_structs.h"
#include "renderer_device.h"
#include "renderer_image_view.h"

#include <vector>

namespace hyperion {

class RendererSwapchain {
    VkSurfaceFormatKHR ChooseSurfaceFormat();
    VkPresentModeKHR ChoosePresentMode();
    VkExtent2D ChooseSwapchainExtent();

    void RetrieveImageHandles();
    RendererResult CreateImageViews();
    void DestroyImageViews();

public:
    struct DepthBuffer {
        std::unique_ptr<RendererImageView> image_view;
        std::unique_ptr<RendererImage> image;
        //VkImage image;
        //VkDeviceMemory memory;

        DepthBuffer() : image_view{}, image{} {}
        DepthBuffer(const DepthBuffer &other) = delete;
        DepthBuffer &operator=(const DepthBuffer &other) = delete;
        ~DepthBuffer() = default;
    };

    RendererSwapchain(RendererDevice *_device, const SwapchainSupportDetails &_details);
    ~RendererSwapchain() = default;

    void Destroy();

    RendererResult Create(const VkSurfaceKHR &surface, QueueFamilyIndices qf_indices);
    RendererResult CreateFramebuffers(VkRenderPass *renderpass);

    std::vector<VkFramebuffer> framebuffers;

    VkSwapchainKHR swapchain = nullptr;
    VkImageUsageFlags image_usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    VkPresentModeKHR present_mode;
    VkExtent2D extent;
    VkSurfaceFormatKHR surface_format;

    VkFormat image_format;

    DepthBuffer depth_buffer;

private:
    RendererDevice *renderer_device = nullptr;
    SwapchainSupportDetails support_details;

    std::vector<VkImage> images;
    std::vector<std::unique_ptr<RendererImageView>> image_views;
};
}; // namespace hyperion


#endif //HYPERION_RENDERER_SWAPCHAIN_H

