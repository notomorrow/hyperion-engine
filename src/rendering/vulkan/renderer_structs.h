//
// Created by emd22 on 2022-02-20.
//

#ifndef HYPERION_RENDERER_STRUCTS_H
#define HYPERION_RENDERER_STRUCTS_H

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>
#include <optional>

namespace hyperion {

struct RendererMeshBindingDescription {
    uint32_t binding;
    uint32_t stride;
    VkVertexInputRate input_rate;

    RendererMeshBindingDescription()
        : binding(0), stride(0), input_rate(VK_VERTEX_INPUT_RATE_VERTEX)
    {
    }

    RendererMeshBindingDescription(uint32_t binding, uint32_t stride, VkVertexInputRate input_rate)
        : binding(binding), stride(stride), input_rate(input_rate)
    {
    }

    VkVertexInputBindingDescription GetBindingDescription() {
        VkVertexInputBindingDescription bind;
        bind.binding = this->binding;
        bind.stride = this->stride;
        bind.inputRate = this->input_rate;
        return bind;
    }
};

struct RendererMeshInputAttribute {
    uint32_t    location;
    uint32_t    binding;
    VkFormat    format;
    uint32_t    offset;

    RendererMeshInputAttribute(uint32_t binding, uint32_t location, uint32_t offset, VkFormat format)
        : location(location), binding(binding), offset(offset), format(format)
    {
    }

    VkVertexInputAttributeDescription GetAttributeDescription() {
        VkVertexInputAttributeDescription attrib;
        attrib.location = this->location;
        attrib.binding = this->binding;
        attrib.format = this->format;
        attrib.offset = this->offset;
        return attrib;
    }
};

struct QueueFamilyIndices {
    using Index_t = uint32_t;

    std::optional<Index_t> graphics_family;
    std::optional<Index_t> transfer_family;
    std::optional<Index_t> present_family;

    const bool IsComplete() {
        return this->graphics_family.has_value()
            && this->transfer_family.has_value()
            && this->present_family.has_value();
    }
};

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

} // namespace hyperion

#endif //HYPERION_RENDERER_STRUCTS_H
