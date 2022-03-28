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
#include <set>
#include <memory>
#include <utility>
#include <tuple>

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

    inline bool operator<(const MeshInputAttribute &other) const
        { return location < other.location; }

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
    MeshInputAttributeSet(const MeshInputAttributeSet &other)
        : attributes(other.attributes) {}

    MeshInputAttributeSet &operator=(const MeshInputAttributeSet &other)
    {
        attributes = other.attributes;

        return *this;
    }

    explicit MeshInputAttributeSet(const std::vector<MeshInputAttribute> &attributes)
        : attributes(attributes)
    {
        SortAttributes();
    }

    explicit MeshInputAttributeSet(uint64_t type_flags)
    {
        for (size_t i = 0; i < MeshInputAttribute::mapping.Size(); i++) {
            uint64_t flag_mask = MeshInputAttribute::mapping.OrdinalToEnum(i);

            if (type_flags & flag_mask) {
                attributes.push_back(MeshInputAttribute::mapping.Get(MeshInputAttribute::Type(flag_mask)));
            }
        }

        SortAttributes();
    }

    ~MeshInputAttributeSet() = default;

    /*uint64_t GetBitMask() const
    {
        uint64_t bit_mask = 0;

        for (uint32_t i = 0; i < MeshInputAttribute::mapping.Size(); i++) {
            uint64_t flag_mask = MeshInputAttribute::mapping.OrdinalToEnum(i);

            if (type_flags & flag_mask) {
                attributes.push_back(MeshInputAttribute::mapping.Get(MeshInputAttribute::Type(flag_mask)));
            }
        }

        return bit_mask;
    }*/

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

    void Merge(const MeshInputAttributeSet &other)
    {
        std::set<MeshInputAttribute> merged_attributes(attributes.begin(), attributes.end());
        merged_attributes.insert(other.attributes.begin(), other.attributes.end());
        std::copy(merged_attributes.begin(), merged_attributes.end(), attributes.begin());

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



template<class ...Args>
class PerFrameData {
    struct FrameDataWrapper {
        std::tuple<std::unique_ptr<Args>...> tup;

        template <class T>
        T *Get()
        {
            return std::get<std::unique_ptr<T>>(tup).get();
        }

        template <class T>
        const T *Get() const
        {
            return std::get<std::unique_ptr<T>>(tup).get();
        }

        template <class T>
        void Set(std::unique_ptr<T> &&value)
        {
            std::get<std::unique_ptr<T>>(tup) = std::move(value);
        }
    };

public:
    PerFrameData(uint32_t num_frames) : m_num_frames(num_frames)
        { m_data.resize(num_frames); }

    PerFrameData(const PerFrameData &other) = delete;
    PerFrameData &operator=(const PerFrameData &other) = delete;
    PerFrameData(PerFrameData &&) = default;
    PerFrameData &operator=(PerFrameData &&) = default;
    ~PerFrameData() = default;

    inline uint32_t NumFrames() const
        { return m_num_frames; }

    inline FrameDataWrapper &operator[](uint32_t index)
        { return m_data[index]; }

    inline const FrameDataWrapper &operator[](uint32_t index) const
        { return m_data[index]; }

    inline FrameDataWrapper &At(uint32_t index)
        { return m_data[index]; }

    inline const FrameDataWrapper &At(uint32_t index) const
        { return m_data[index]; }

    inline void Reset()
        { m_data = std::vector<FrameDataWrapper>(m_num_frames); }

protected:
    uint32_t m_num_frames;
    std::vector<FrameDataWrapper> m_data;
};

} // namespace renderer
} // namespace hyperion

#endif //HYPERION_RENDERER_STRUCTS_H
