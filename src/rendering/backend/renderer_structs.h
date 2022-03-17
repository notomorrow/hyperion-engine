//
// Created by emd22 on 2022-02-20.
//

#ifndef HYPERION_RENDERER_STRUCTS_H
#define HYPERION_RENDERER_STRUCTS_H

#include <util/enum_options.h>

#include <vulkan/vulkan.h>
#include <hash_code.h>

#include <cstdint>
#include <vector>
#include <algorithm>
#include <optional>

namespace hyperion {
namespace renderer {

using ::std::optional;

struct MeshBindingDescription {
    uint32_t binding;
    uint32_t stride;
    VkVertexInputRate input_rate;

    MeshBindingDescription()
        : binding(0), stride(0), input_rate(VK_VERTEX_INPUT_RATE_VERTEX)
    {
    }

    MeshBindingDescription(uint32_t binding, uint32_t stride, VkVertexInputRate input_rate)
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

struct MeshInputAttribute {
    enum Type {
        MESH_INPUT_ATTRIBUTE_UNDEFINED    = 0,
        MESH_INPUT_ATTRIBUTE_POSITION     = 1,
        MESH_INPUT_ATTRIBUTE_NORMAL       = 2,
        MESH_INPUT_ATTRIBUTE_TEXCOORD0    = 4,
        MESH_INPUT_ATTRIBUTE_TEXCOORD1    = 8,
        MESH_INPUT_ATTRIBUTE_TANGENT      = 16,
        MESH_INPUT_ATTRIBUTE_BITANGENT    = 32,
        MESH_INPUT_ATTRIBUTE_BONE_INDICES = 64,
        MESH_INPUT_ATTRIBUTE_BONE_WEIGHTS = 128,
    };

    static const EnumOptions<Type, MeshInputAttribute, 16> mapping;

    uint32_t    location;
    uint32_t    binding;
    // total size -- num elements * sizeof(float)
    size_t      size;

    inline VkFormat GetFormat() const
    {
        switch (this->size) {
        case sizeof(float): return VK_FORMAT_R32_SFLOAT;
        case sizeof(float) * 2: return VK_FORMAT_R32G32_SFLOAT;
        case sizeof(float) * 3: return VK_FORMAT_R32G32B32_SFLOAT;
        case sizeof(float) * 4: return VK_FORMAT_R32G32B32A32_SFLOAT;
        default: AssertThrowMsg(0, "Unsupported vertex attribute format!");
        }
    }

    inline bool operator<(const MeshInputAttribute &other) const
        { return location < other.location; }

    VkVertexInputAttributeDescription GetAttributeDescription() {
        VkVertexInputAttributeDescription attrib{};
        attrib.location = this->location;
        attrib.binding = this->binding;
        attrib.format = this->GetFormat();

        return attrib;
    }

    inline HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(this->location);
        hc.Add(this->binding);
        hc.Add(this->size);

        return hc;
    }
};

struct MeshInputAttributeSet {
    std::vector<MeshInputAttribute> attributes;

    MeshInputAttributeSet() {}

    explicit MeshInputAttributeSet(const std::vector<MeshInputAttribute> &attributes)
        : attributes(attributes)
    {
        SortAttributes();
    }

    explicit MeshInputAttributeSet(uint32_t type_flags)
    {
        for (uint32_t i = 0; i < MeshInputAttribute::mapping.Size(); i++) {
            uint64_t flag_mask = MeshInputAttribute::mapping.OrdinalToEnum(i);

            if (type_flags & flag_mask) {
                attributes.push_back(MeshInputAttribute::mapping.Get(MeshInputAttribute::Type(flag_mask)));
            }
        }

        SortAttributes();
    }

    MeshInputAttributeSet(const MeshInputAttributeSet &other)
        : attributes(other.attributes) {}

    ~MeshInputAttributeSet() = default;

    void AddAttributes(const std::vector<MeshInputAttribute> &_attributes)
    {
        for (const auto &attribute : _attributes) {
            attributes.push_back(attribute);
        }

        SortAttributes();
    }

    void AddAttribute(const MeshInputAttribute &attribute)
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

    optional<Index_t> graphics_family;
    optional<Index_t> transfer_family;
    optional<Index_t> present_family;
    optional<Index_t> compute_family;

    const bool IsComplete() {
        return this->graphics_family.has_value()
            && this->transfer_family.has_value()
            && this->present_family.has_value()
            && this->compute_family.has_value();
    }
};

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkQueueFamilyProperties> queue_family_properties;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

} // namespace renderer
} // namespace hyperion

#endif //HYPERION_RENDERER_STRUCTS_H
