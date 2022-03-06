//
// Created by emd22 on 2022-02-20.
//

#ifndef HYPERION_RENDERER_STRUCTS_H
#define HYPERION_RENDERER_STRUCTS_H

#include <vulkan/vulkan.h>
#include <hash_code.h>

#include <cstdint>
#include <vector>
#include <algorithm>
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
    // total size -- num elements * sizeof(float)
    size_t      size;
    // vk format
    VkFormat    format;

    RendererMeshInputAttribute(uint32_t location, uint32_t binding, size_t size, VkFormat format)
        : location(location),
          binding(binding),
          size(size),
          format(format)
    {
    }

    RendererMeshInputAttribute(const RendererMeshInputAttribute &other)
        : location(other.location),
          binding(other.binding),
          size(other.size),
          format(other.format)
    {
    }

    inline bool operator<(const RendererMeshInputAttribute &other) const
        { return location < other.location; }

    VkVertexInputAttributeDescription GetAttributeDescription() {
        VkVertexInputAttributeDescription attrib{};
        attrib.location = this->location;
        attrib.binding = this->binding;
        attrib.format = this->format;

        return attrib;
    }

    inline HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(location);
        hc.Add(binding);
        hc.Add(uint32_t(format));

        return hc;
    }
};

struct RendererMeshInputAttributeSet {
    std::vector<RendererMeshInputAttribute> attributes;

    RendererMeshInputAttributeSet() {}
    explicit RendererMeshInputAttributeSet(const std::vector<RendererMeshInputAttribute> &attributes)
        : attributes(attributes)
    {
        SortAttributes();
    }
    RendererMeshInputAttributeSet(const RendererMeshInputAttributeSet &other)
        : attributes(other.attributes) {}
    ~RendererMeshInputAttributeSet() = default;

    void AddAttributes(const std::vector<RendererMeshInputAttribute> &_attributes)
    {
        for (const auto &attribute : _attributes) {
            attributes.push_back(attribute);
        }

        SortAttributes();
    }

    void AddAttribute(const RendererMeshInputAttribute &attribute)
    {
        attributes.push_back(attribute);
        SortAttributes();
    }

    void SortAttributes()
    {
        std::sort(attributes.begin(), attributes.end());
    }

    inline size_t TotalSize() const
    {
        size_t size = 0;

        for (auto &attribute : attributes) {
            size += attribute.size;
        }

        return size;
    }

    inline HashCode GetHashCode() const
    {
        HashCode hc;
        
        for (auto &attribute : attributes) {
            hc.Add(attribute.GetHashCode());
        }

        return hc;
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
