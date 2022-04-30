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

enum class DatumType {
    UNSIGNED_BYTE,
    SIGNED_BYTE,
    UNSIGNED_SHORT,
    SIGNED_SHORT,
    UNSIGNED_INT,
    SIGNED_INT,
    FLOAT
};

enum class CullMode {
    NONE,
    BACK,
    FRONT
};

enum class FillMode {
    FILL,
    LINE
};

enum class Topology {
    TRIANGLES,
    TRIANGLE_FAN,
    TRIANGLE_STRIP,

    LINES,

    POINTS
};

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

    VkVertexInputBindingDescription GetBindingDescription()
    {
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
        case sizeof(float):     return VK_FORMAT_R32_SFLOAT;
        case sizeof(float) * 2: return VK_FORMAT_R32G32_SFLOAT;
        case sizeof(float) * 3: return VK_FORMAT_R32G32B32_SFLOAT;
        case sizeof(float) * 4: return VK_FORMAT_R32G32B32A32_SFLOAT;
        default: AssertThrowMsg(0, "Unsupported vertex attribute format!");
        }
    }

    VkVertexInputAttributeDescription GetAttributeDescription() const {
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
    uint64_t flag_mask;

    MeshInputAttributeSet()
        : flag_mask(0) {}
    MeshInputAttributeSet(uint64_t flag_mask)
        : flag_mask(flag_mask) {}
    MeshInputAttributeSet(const MeshInputAttributeSet &other)
        : flag_mask(other.flag_mask) {}

    MeshInputAttributeSet &operator=(const MeshInputAttributeSet &other)
    {
        flag_mask = other.flag_mask;

        return *this;
    }

    ~MeshInputAttributeSet() = default;

    explicit operator bool() const { return bool(flag_mask); }

    MeshInputAttributeSet operator~() const { return ~flag_mask; }
    MeshInputAttributeSet operator&(uint64_t flags) const { return {flag_mask & flags}; }
    MeshInputAttributeSet &operator&=(uint64_t flags) { flag_mask &= flags; return *this; }
    MeshInputAttributeSet operator|(uint64_t flags) const { return {flag_mask | flags}; }
    MeshInputAttributeSet &operator|=(uint64_t flags) { flag_mask |= flags; return *this; }

    bool Has(MeshInputAttribute::Type type) const { return bool(operator&(uint64_t(type))); }

    void Set(uint64_t flags, bool enable = true)
    {
        if (enable) {
            flag_mask |= flags;
        } else {
            flag_mask &= ~flags;
        }
    }

    void Set(MeshInputAttribute::Type type, bool enable = true)
    {
        Set(uint64_t(type), enable);
    }

    void Merge(const MeshInputAttributeSet &other)
    {
        flag_mask |= other.flag_mask;
    }

    std::vector<MeshInputAttribute> BuildAttributes() const
    {
        std::vector<MeshInputAttribute> attributes;
        attributes.reserve(MeshInputAttribute::mapping.Size());

        for (size_t i = 0; i < MeshInputAttribute::mapping.Size(); i++) {
            const uint64_t iter_flag_mask = MeshInputAttribute::mapping.OrdinalToEnum(i);  // NOLINT(readability-static-accessed-through-instance)

            if (flag_mask & iter_flag_mask) {
                attributes.push_back(MeshInputAttribute::mapping[MeshInputAttribute::Type(iter_flag_mask)]);
            }
        }

        return attributes;
    }

    inline size_t VertexSize() const
    {
        size_t size = 0;

        for (size_t i = 0; i < MeshInputAttribute::mapping.Size(); i++) {
            const uint64_t iter_flag_mask = MeshInputAttribute::mapping.OrdinalToEnum(i);  // NOLINT(readability-static-accessed-through-instance)

            if (flag_mask & iter_flag_mask) {
                size += MeshInputAttribute::mapping[MeshInputAttribute::Type(iter_flag_mask)].size;
            }
        }

        return size;
    }

    inline HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(flag_mask);

        return hc;
    }
};

struct QueueFamilyIndices {
    using Index = uint32_t;
    
    std::optional<Index> graphics_family;
    std::optional<Index> transfer_family;
    std::optional<Index> present_family;
    std::optional<Index> compute_family;

    bool IsComplete() const {
        return graphics_family.has_value()
            && transfer_family.has_value()
            && present_family.has_value()
            && compute_family.has_value();
    }
};

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkQueueFamilyProperties> queue_family_properties;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

struct Extent2D {
    uint32_t width, height;

    Extent2D()
        : width(0),
          height(0)
    {
    }

    Extent2D(uint32_t width, uint32_t height)
        : width(width),
          height(height)
    {
    }

    Extent2D(const Extent2D &other) = default;
    Extent2D &operator=(const Extent2D &other) = default;
    Extent2D(Extent2D &&other) noexcept = default;
    Extent2D &operator=(Extent2D &&other) noexcept = default;
    ~Extent2D() = default;

    bool operator==(const Extent2D &other) const
    {
        return width == other.width
            && height == other.height;
    }

    bool operator!=(const Extent2D &other) const
        { return !operator==(other); }
};

struct Extent3D {
    uint32_t width, height, depth;

    Extent3D()
        : width(0),
          height(0),
          depth(0)
    {
    }

    Extent3D(uint32_t width, uint32_t height, uint32_t depth)
        : width(width),
          height(height),
          depth(depth)
    {
    }

    explicit Extent3D(const Extent2D &extent_2d, uint32_t depth = 1)
        : width(extent_2d.width),
          height(extent_2d.height),
          depth(depth)
    {
    }

    Extent3D(const Extent3D &other) = default;
    Extent3D &operator=(const Extent3D &other) = default;
    Extent3D(Extent3D &&other) noexcept = default;
    Extent3D &operator=(Extent3D &&other) noexcept = default;
    ~Extent3D() = default;

    bool operator==(const Extent3D &other) const
    {
        return width == other.width
            && height == other.height
            && depth == other.depth;
    }

    bool operator!=(const Extent3D &other) const
        { return !operator==(other); }
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
