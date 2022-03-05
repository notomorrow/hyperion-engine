//
// Created by emd22 on 2022-02-20.
//

#ifndef HYPERION_RENDERER_DEVICE_H
#define HYPERION_RENDERER_DEVICE_H

#include <vector>
#include <set>
#include <optional>

#include "renderer_structs.h"
#include "renderer_features.h"

namespace hyperion {

class RendererDevice {
public:
    RendererDevice();
    ~RendererDevice();
    void Destroy();

    void SetDevice(const VkDevice &_device);
    void SetPhysicalDevice(VkPhysicalDevice);
    void SetRenderSurface(const VkSurfaceKHR &_surface);
    void SetRequiredExtensions(const std::vector<const char *> &_extensions);
    VkDevice         GetDevice();
    VkSurfaceKHR     GetRenderSurface();
    VkPhysicalDevice GetPhysicalDevice();
    std::vector<const char *> GetRequiredExtensions();

    QueueFamilyIndices FindQueueFamilies();
    SwapchainSupportDetails QuerySwapchainSupport();

    inline const RendererFeatures &GetRendererFeatures() const { return renderer_features; }

    VkQueue GetQueue(QueueFamilyIndices::Index_t queue_family_index, uint32_t queue_index = 0);

    RendererResult CreateLogicalDevice(const std::set<uint32_t> &required_queue_families, const std::vector<const char *> &required_extensions);

    RendererResult CheckDeviceSuitable();

    std::vector<const char *> CheckExtensionSupport(std::vector<const char *> _extensions);
    std::vector<const char *> CheckExtensionSupport();

    std::vector<VkExtensionProperties> GetSupportedExtensions();
private:
    VkDevice                   device;
    VkSurfaceKHR               surface;
    VkPhysicalDevice           physical;

    RendererFeatures renderer_features;

    std::vector<const char *> required_extensions;
};

} // namespace hyperion

#endif //HYPERION_RENDERER_DEVICE_H
