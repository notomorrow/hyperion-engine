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
        
        m_indexing_features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT, nullptr};
        m_features2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &m_indexing_features};

        vkGetPhysicalDeviceFeatures2(m_physical_device, &m_features2);

        vkGetPhysicalDeviceMemoryProperties(physical_device, &m_memory_properties);
    }
}

} // namespace renderer
} // namespace hyperion