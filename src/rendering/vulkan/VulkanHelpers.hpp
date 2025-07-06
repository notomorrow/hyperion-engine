/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_HELPERS_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_HELPERS_HPP

#include <rendering/vulkan/VulkanDevice.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/RenderStructs.hpp>

#include <vulkan/vulkan.h>

#include <Types.hpp>

namespace hyperion {
enum class DescriptorSetElementType : uint32;

namespace helpers {

VkIndexType ToVkIndexType(GpuElemType);
VkFormat ToVkFormat(TextureFormat);
VkFilter ToVkFilter(TextureFilterMode);
VkSamplerAddressMode ToVkSamplerAddressMode(TextureWrapMode);
VkImageAspectFlags ToVkImageAspect(TextureFormat internalFormat);
VkImageType ToVkImageType(TextureType);
VkImageViewType ToVkImageViewType(TextureType type);
VkDescriptorType ToVkDescriptorType(DescriptorSetElementType type);

} // namespace helpers

namespace platform {

template <>
struct SingleTimeCommandsPlatformImpl<Platform::vulkan>
{
    SingleTimeCommands<Platform::vulkan>* self = nullptr;
    VkCommandPool pool {};
    QueueFamilyIndices familyIndices {};
};

} // namespace platform

} // namespace hyperion

#endif