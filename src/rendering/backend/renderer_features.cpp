#include "renderer_features.h"

namespace hyperion {
namespace renderer {
Features::Features()
    : m_physical_device(nullptr),
      m_properties({}),
      m_features({})
{
}

Features::Features(VkPhysicalDevice physical_device)
    : Features()
{
    SetPhysicalDevice(physical_device);
}

void Features::SetPhysicalDevice(VkPhysicalDevice physical_device)
{
    if ((m_physical_device = physical_device)) {
        vkGetPhysicalDeviceProperties(physical_device, &m_properties);
        vkGetPhysicalDeviceFeatures(physical_device, &m_features);
    }
}

} // namespace renderer
} // namespace hyperion