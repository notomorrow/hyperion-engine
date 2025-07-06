/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/FixedArray.hpp>
#include <Types.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
enum class VulkanDeviceQueueType : uint8
{
    GRAPHICS,
    COMPUTE,
    TRANSFER,
    PRESENT
};

struct VulkanDeviceQueue
{
    VulkanDeviceQueueType type;
    VkQueue queue;
    FixedArray<VkCommandPool, 8> commandPools;
};

} // namespace hyperion
