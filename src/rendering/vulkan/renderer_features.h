#ifndef HYPERION_RENDERER_FEATURES_H
#define HYPERION_RENDERER_FEATURES_H

#include "renderer_result.h"

#include <vulkan/vulkan.h>

#include <array>

namespace hyperion {

class RendererFeatures {
public:
    RendererFeatures();
    RendererFeatures(VkPhysicalDevice);

    RendererFeatures(const RendererFeatures &other) = delete;
    RendererFeatures &operator=(const RendererFeatures &other) = delete;
    ~RendererFeatures() = default;

    void SetPhysicalDevice(VkPhysicalDevice);

    inline bool IsDiscreteGpu() const { return m_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU; }

    inline const char *GetDeviceName() const { return m_properties.deviceName; }

    inline const VkPhysicalDeviceProperties &GetPhysicalDeviceProperties() const
        { return m_properties; }

    inline const VkPhysicalDeviceFeatures &GetPhysicalDeviceFeatures() const
        { return m_features; }

    struct DeviceRequirementsResult {
        enum {
            DEVICE_REQUIREMENTS_OK = 0,
            DEVICE_REQUIREMENTS_ERR = 1
        } result;

        const char *message;

        DeviceRequirementsResult(decltype(result) result, const char *message = "")
            : result(result), message(message) {}
        DeviceRequirementsResult(const DeviceRequirementsResult &other)
            : result(other.result), message(other.message) {}
        inline operator bool() const { return result == DEVICE_REQUIREMENTS_OK; }
    };

#define REQUIRES_VK_FEATURE(feature) \
    do { \
        if (!(feature)) { \
            return DeviceRequirementsResult(DeviceRequirementsResult::DEVICE_REQUIREMENTS_ERR, "Feature constraint " #feature " not satisfied."); \
        } \
    } while (0)

    DeviceRequirementsResult SatisfiesMinimumRequirements()
    {
        REQUIRES_VK_FEATURE(m_features.geometryShader);
        REQUIRES_VK_FEATURE(m_properties.limits.maxDescriptorSetSamplers >= 16);
        REQUIRES_VK_FEATURE(m_properties.limits.maxDescriptorSetUniformBuffers >= 16);

        return DeviceRequirementsResult(DeviceRequirementsResult::DEVICE_REQUIREMENTS_OK);
    }

#undef REQUIRES_VK_FEATURE

    /* get the first supported format out of the provided list of format choices. */
    template <size_t Size>
    inline VkFormat GetSupportedFormat(std::array<VkFormat, Size> possible_formats, VkImageTiling tiling, VkFormatFeatureFlags features) const
    {
        static_assert(Size > 0, "Size must be greater than zero!");

        if (m_physical_device == nullptr) {
            return VK_FORMAT_UNDEFINED;
        }

        for (size_t i = 0; i < Size; i++) {
            VkFormat format = possible_formats[i];

            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(m_physical_device, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }

        return VK_FORMAT_UNDEFINED;
    }

    inline RendererResult GetImageFormatProperties(VkFormat format,
        VkImageType             type,
        VkImageTiling           tiling,
        VkImageUsageFlags       usage,
        VkImageCreateFlags      flags,
        VkImageFormatProperties *out_properties) const
    {
        if (m_physical_device == nullptr) {
            return RendererResult(RendererResult::RENDERER_ERR, "Cannot find supported format; physical device is not initialized.");
        }

        if (vkGetPhysicalDeviceImageFormatProperties(m_physical_device, format, type, tiling, usage, flags, out_properties) != VK_SUCCESS) {
            return RendererResult(RendererResult::RENDERER_ERR, "Failed to get image format properties");
        }

        HYPERION_RETURN_OK;
    }

    inline bool IsImageFormatSupported(VkFormat format,
        VkImageType             type,
        VkImageTiling           tiling,
        VkImageUsageFlags       usage,
        VkImageCreateFlags      flags) const
    {
        VkImageFormatProperties tmp;

        return GetImageFormatProperties(format, type, tiling, usage, flags, &tmp).result == RendererResult::RENDERER_OK;
    }

private:
    VkPhysicalDevice m_physical_device;
    VkPhysicalDeviceProperties m_properties;
    VkPhysicalDeviceFeatures m_features;
};

} // namespace hyperion

#endif