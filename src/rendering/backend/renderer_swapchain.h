//
// Created by emd22 on 2022-02-20.
//

#ifndef HYPERION_RENDERER_SWAPCHAIN_H
#define HYPERION_RENDERER_SWAPCHAIN_H

#include "renderer_structs.h"
#include "renderer_device.h"
#include "renderer_image_view.h"
#include "renderer_semaphore.h"
#include "renderer_fbo.h"

#include <vector>

namespace hyperion {
namespace renderer {
class Swapchain {
    static constexpr VkImageUsageFlags image_usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkSurfaceFormatKHR ChooseSurfaceFormat();
    VkPresentModeKHR GetPresentMode();
    VkExtent2D ChooseSwapchainExtent();
    void RetrieveSupportDetails(Device *device);
    void RetrieveImageHandles(Device *device);
    Result CreateSemaphores(Device *device);

public:
    Swapchain();
    ~Swapchain() = default;

    Result Create(Device *device, const VkSurfaceKHR &surface);
    Result Destroy(Device *device);

    inline size_t GetNumImages() const { return this->images.size(); }
    inline SemaphoreChain &GetPresentSemaphores() { return present_semaphores; }
    inline const SemaphoreChain &GetPresentSemaphores() const { return present_semaphores; }

    VkSwapchainKHR swapchain;
    VkExtent2D extent;
    VkSurfaceFormatKHR surface_format;
    VkFormat image_format;
    std::vector<VkImage> images;

private:
    inline uint32_t GetMinImageCount() const { return this->support_details.capabilities.minImageCount; }
    inline uint32_t GetMaxImageCount() const { return this->support_details.capabilities.maxImageCount; }

    SemaphoreChain          present_semaphores;
    SwapchainSupportDetails support_details;
    VkPresentModeKHR        present_mode;
};

} // namespace renderer
}; // namespace hyperion

#endif //HYPERION_RENDERER_SWAPCHAIN_H

