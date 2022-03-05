#include "renderer_features.h"

namespace hyperion {

RendererFeatures::RendererFeatures()
    : m_physical_device(nullptr),
      m_properties({}),
      m_features({})
{
}

RendererFeatures::RendererFeatures(VkPhysicalDevice physical_device)
    : RendererFeatures()
{
    SetPhysicalDevice(physical_device);
}

void RendererFeatures::SetPhysicalDevice(VkPhysicalDevice physical_device)
{
    if ((m_physical_device = physical_device)) {
        vkGetPhysicalDeviceProperties(physical_device, &m_properties);
        vkGetPhysicalDeviceFeatures(physical_device, &m_features);
    }
}

} // namespace hyperion