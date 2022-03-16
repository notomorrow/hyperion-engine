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

#include "../../system/vma/vma_usage.h"

namespace hyperion {
namespace renderer {

using ::std::vector,
      ::std::set;

struct Instance;

class Device {
public:
    Device();
    ~Device();
    void Destroy();

    void SetDevice(const VkDevice &_device);
    void SetPhysicalDevice(VkPhysicalDevice);
    void SetRenderSurface(const VkSurfaceKHR &_surface);
    void SetRequiredExtensions(const vector<const char *> &_extensions);
    VkDevice         GetDevice();
    VkSurfaceKHR     GetRenderSurface();
    VkPhysicalDevice GetPhysicalDevice();
    vector<const char *> GetRequiredExtensions();

    Result SetupAllocator(Instance *instance);
    Result DestroyAllocator();

    QueueFamilyIndices FindQueueFamilies();
    SwapchainSupportDetails QuerySwapchainSupport();

    inline const Features &GetFeatures() const { return renderer_features; }

    VkQueue GetQueue(QueueFamilyIndices::Index_t queue_family_index, uint32_t queue_index = 0);

    Result CreateLogicalDevice(const set<uint32_t> &required_queue_families, const vector<const char *> &required_extensions);

    Result CheckDeviceSuitable();

    VmaAllocator *GetAllocator() { return &this->allocator; }

    vector<const char *> CheckExtensionSupport(vector<const char *> _extensions);
    vector<const char *> CheckExtensionSupport();

    vector<VkExtensionProperties> GetSupportedExtensions();

private:
    VkDevice                   device;
    VkSurfaceKHR               surface;
    VkPhysicalDevice           physical;

    VmaAllocator allocator;

    Features renderer_features;

    vector<const char *> required_extensions;
};

} // namespace renderer
} // namespace hyperion

#endif //HYPERION_RENDERER_DEVICE_H
