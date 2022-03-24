#ifndef HYPERION_RENDERER_FEATURES_H
#define HYPERION_RENDERER_FEATURES_H

#include "renderer_result.h"
#include "renderer_image.h"

#include <vulkan/vulkan.h>

#include <array>

namespace hyperion {
namespace renderer {

class Features {
public:
    Features();
    Features(VkPhysicalDevice);

    Features(const Features &other) = delete;
    Features &operator=(const Features &other) = delete;
    ~Features() = default;

    void SetPhysicalDevice(VkPhysicalDevice);
    inline VkPhysicalDevice GetPhysicalDevice() const { return m_physical_device; }

    inline bool IsDiscreteGpu() const { return m_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU; }

    inline const char *GetDeviceName() const { return m_properties.deviceName; }

    inline const VkPhysicalDeviceProperties &GetPhysicalDeviceProperties() const
    {
        return m_properties;
    }

    inline const VkPhysicalDeviceFeatures &GetPhysicalDeviceFeatures() const
    {
        return m_features;
    }

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
        REQUIRES_VK_FEATURE(m_features.fragmentStoresAndAtomics); /* for imageStore() in fragment shaders */
        REQUIRES_VK_FEATURE(m_features.shaderSampledImageArrayDynamicIndexing); /* for accessing textures based on dynamic index (push constant) */
        REQUIRES_VK_FEATURE(m_properties.limits.maxDescriptorSetSamplers >= 16);
        REQUIRES_VK_FEATURE(m_properties.limits.maxDescriptorSetUniformBuffers >= 16);

        return DeviceRequirementsResult(DeviceRequirementsResult::DEVICE_REQUIREMENTS_OK);
    }

#undef REQUIRES_VK_FEATURE

    inline bool IsSupportedFormat(VkFormat format, VkImageTiling tiling, VkFormatFeatureFlags features) const
    {
        if (m_physical_device == nullptr) {
            return false;
        }

        DebugLog(
            LogType::Debug,
            "Checking support for Vulkan format %d with tiling mode %d and feature flags %d.\n",
            format,
            tiling,
            features
        );

        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(m_physical_device, format, &props);

        const VkFormatFeatureFlags flags = (tiling == VK_IMAGE_TILING_LINEAR)
            ? props.linearTilingFeatures
            : (tiling == VK_IMAGE_TILING_OPTIMAL) ? props.optimalTilingFeatures : 0;

        if ((flags & features) == features) {
            DebugLog(
                LogType::Debug,
                "Vulkan format %d with tiling mode %d and feature flags %d support found!\n",
                format,
                tiling,
                features
            );

            return true;
        }

        DebugLog(
            LogType::Debug,
            "Vulkan format %d with tiling mode %d and feature flags %d not supported.\n",
            format,
            tiling,
            features
        );

        return false;
    }

    /* get the first supported format out of the provided list of format choices. */
    template <size_t Size>
    inline Image::InternalFormat FindSupportedFormat(const std::array<Image::InternalFormat, Size> &possible_formats, VkImageTiling tiling, VkFormatFeatureFlags features) const
    {
        static_assert(Size > 0, "Size must be greater than zero!");

        DebugLog(
            LogType::Debug,
            "Looking for format to use with tiling option %d and format features %d. First choice: %d\n",
            tiling,
            features,
            Image::ToVkFormat(possible_formats[0])
        );

        if (m_physical_device == nullptr) {
            DebugLog(LogType::Debug, "No physical device set -- cannot find supported format!\n");
            return Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_NONE;
        }

        for (size_t i = 0; i < Size; i++) {
            if (IsSupportedFormat(Image::ToVkFormat(possible_formats[i]), tiling, features) != VK_FORMAT_UNDEFINED) {
                return possible_formats[i];
            }
        }

        return Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_NONE;
    }

    /* get the first supported format out of the provided list of format choices. */
    template <size_t Size>
    inline VkFormat FindSupportedFormat(const std::array<VkFormat, Size> &possible_formats, VkImageTiling tiling, VkFormatFeatureFlags features) const
    {
        static_assert(Size > 0, "Size must be greater than zero!");

        DebugLog(
            LogType::Debug,
            "Looking for format to use with tiling option %d and format features %d. First choice: %d\n",
            tiling,
            features,
            possible_formats[0]
        );

        if (m_physical_device == nullptr) {
            DebugLog(LogType::Debug, "No physical device set -- cannot find supported format!\n");
            return VK_FORMAT_UNDEFINED;
        }

        for (size_t i = 0; i < Size; i++) {
            VkFormat format = possible_formats[i];

            if (IsSupportedFormat(format, tiling, features)) {
                return format;
            }
        }

        return VK_FORMAT_UNDEFINED;
    }

    inline Result GetImageFormatProperties(VkFormat format,
        VkImageType             type,
        VkImageTiling           tiling,
        VkImageUsageFlags       usage,
        VkImageCreateFlags      flags,
        VkImageFormatProperties *out_properties) const
    {
        if (m_physical_device == nullptr) {
            return Result(Result::RENDERER_ERR, "Cannot find supported format; physical device is not initialized.");
        }

        if (vkGetPhysicalDeviceImageFormatProperties(m_physical_device, format, type, tiling, usage, flags, out_properties) != VK_SUCCESS) {
            return Result(Result::RENDERER_ERR, "Failed to get image format properties");
        }

        HYPERION_RETURN_OK;
    }

    inline bool IsSupportedImageFormat(VkFormat format,
        VkImageType             type,
        VkImageTiling           tiling,
        VkImageUsageFlags       usage,
        VkImageCreateFlags      flags) const
    {
        VkImageFormatProperties tmp;

        return GetImageFormatProperties(format, type, tiling, usage, flags, &tmp).result == Result::RENDERER_OK;
    }

    template <class StructType>
    constexpr size_t PaddedSize() const
    {
        // Calculate required alignment based on minimum device offset alignment
        constexpr size_t alignment = m_properties.limits.minUniformBufferOffsetAlignment;
        size_t size = sizeof(StructType);

        if (alignment != 0) {
            size = (size + alignment - 1) & ~(alignment - 1);
        }

        return size;

    }

private:
    VkPhysicalDevice m_physical_device;
    VkPhysicalDeviceProperties m_properties;
    VkPhysicalDeviceFeatures m_features;
};

} // namespace renderer
} // namespace hyperion

#endif