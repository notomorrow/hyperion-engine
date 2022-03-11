//
// Created by emd22 on 2022-02-20.
//

#ifndef HYPERION_RENDERER_SWAPCHAIN_H
#define HYPERION_RENDERER_SWAPCHAIN_H

#include "renderer_structs.h"
#include "renderer_device.h"
#include "renderer_image_view.h"
#include "renderer_fbo.h"

#include <vector>

namespace hyperion {
namespace renderer {
class Swapchain {
    VkSurfaceFormatKHR ChooseSurfaceFormat();
    VkPresentModeKHR GetPresentMode();
    VkExtent2D ChooseSwapchainExtent();
    void RetrieveSupportDetails(Device *device);

    void RetrieveImageHandles(Device *device);

public:
    Swapchain();
    ~Swapchain() = default;

    Result Create(Device *device, const VkSurfaceKHR &surface);
    Result Destroy(Device *device);

    inline size_t GetNumImages() const { return this->images.size(); }

    std::vector<std::unique_ptr<FramebufferObject>> framebuffers;

    VkSwapchainKHR swapchain = nullptr;
    VkImageUsageFlags image_usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    VkPresentModeKHR present_mode;
    VkExtent2D extent;
    VkSurfaceFormatKHR surface_format;

    VkFormat image_format;
    
    std::vector<VkImage> images;

private:
    SwapchainSupportDetails support_details;
};

} // namespace renderer
}; // namespace hyperion

#endif //HYPERION_RENDERER_SWAPCHAIN_H

