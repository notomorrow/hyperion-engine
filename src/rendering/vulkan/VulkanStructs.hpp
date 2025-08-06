/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once
#include <core/utilities/Optional.hpp>
#include <core/containers/Array.hpp>
#include <core/Types.hpp>

#include <vulkan/vulkan.h>

#include <memory>

namespace hyperion {
struct MeshBindingDescription
{
    uint32 binding;
    uint32 stride;
    VkVertexInputRate inputRate;

    MeshBindingDescription()
        : binding(0),
          stride(0),
          inputRate(VK_VERTEX_INPUT_RATE_VERTEX)
    {
    }

    MeshBindingDescription(uint32 binding, uint32 stride, VkVertexInputRate inputRate)
        : binding(binding),
          stride(stride),
          inputRate(inputRate)
    {
    }

    VkVertexInputBindingDescription GetBindingDescription()
    {
        VkVertexInputBindingDescription bind;
        bind.binding = this->binding;
        bind.stride = this->stride;
        bind.inputRate = this->inputRate;
        return bind;
    }
};

struct VulkanSwapchainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    Array<VkQueueFamilyProperties> queueFamilyProperties;
    Array<VkSurfaceFormatKHR> formats;
    Array<VkPresentModeKHR> presentModes;
};

struct IndirectDrawCommand
{
    // native vk object
    VkDrawIndexedIndirectCommand command;
};

static_assert(std::is_standard_layout_v<IndirectDrawCommand>, "IndirectDrawCommand must be POD");
static_assert(sizeof(IndirectDrawCommand) == 20, "Verify size of struct in shader");

} // namespace hyperion
