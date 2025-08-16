/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <rendering/vulkan/VulkanDevice.hpp>

#include <rendering/RenderObject.hpp>

#include <system/vma/VmaUsage.hpp>

#include <core/Types.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {

class VulkanInstance
{
    static ExtensionMap GetExtensionMap();

    /* Setup debug mode */
    RendererResult SetupDebug();
    RendererResult SetupDebugMessenger();

public:
    VulkanInstance();

    RendererResult Initialize(bool loadDebugLayers = false);

    HYP_FORCE_INLINE VkInstance GetInstance() const
    {
        return m_instance;
    }

    HYP_FORCE_INLINE const VulkanDeviceRef& GetDevice() const
    {
        return m_device;
    }

    HYP_FORCE_INLINE const VulkanSwapchainRef& GetSwapchain() const
    {
        return m_swapchain;
    }

    HYP_FORCE_INLINE VmaAllocator GetAllocator() const
    {
        return allocator;
    }

    void SetValidationLayers(Array<const char*> validationLayers);

    RendererResult CreateDevice(VkPhysicalDevice _physical_device = nullptr);
    RendererResult CreateSwapchain();
    RendererResult RecreateSwapchain();

    RendererResult Destroy();

    const char* appName;
    const char* engineName;

private:
    void CreateSurface();

    VkInstance m_instance;
    VkSurfaceKHR m_surface;

    VmaAllocator allocator = nullptr;

    VulkanDeviceRef m_device;
    VulkanSwapchainRef m_swapchain;

    Array<const char*> validationLayers;

#ifndef HYPERION_BUILD_RELEASE
    VkDebugUtilsMessengerEXT debugMessenger;
#endif
};

} // namespace hyperion
