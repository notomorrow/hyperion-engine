#ifndef HYPERION_V2_BACKEND_RENDERER_DESCRIPTOR_SET2_HPP
#define HYPERION_V2_BACKEND_RENDERER_DESCRIPTOR_SET2_HPP

#include <core/Name.hpp>
#include <core/lib/Optional.hpp>
#include <core/lib/RefCountedPtr.hpp>
#include <core/lib/ArrayMap.hpp>
#include <core/lib/FixedArray.hpp>
#include <core/lib/Range.hpp>

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/rt/RendererAccelerationStructure.hpp>

#include <util/Defines.hpp>
#include <Types.hpp>
#include <HashCode.hpp>


namespace hyperion {
namespace renderer {

struct DescriptorSetDeclaration;

enum class DescriptorSetElementType : uint32
{
    UNSET,
    UNIFORM_BUFFER,
    UNIFORM_BUFFER_DYNAMIC,
    STORAGE_BUFFER,
    STORAGE_BUFFER_DYNAMIC,
    IMAGE,
    IMAGE_STORAGE,
    SAMPLER,
    TLAS,
    MAX
};

constexpr uint descriptor_set_element_type_to_buffer_type[uint(DescriptorSetElementType::MAX)] = {
    0,                                                          // UNSET
    (1u << uint(GPUBufferType::CONSTANT_BUFFER)),               // UNIFORM_BUFFER
    (1u << uint(GPUBufferType::CONSTANT_BUFFER)),               // UNIFORM_BUFFER_DYNAMIC
    (1u << uint(GPUBufferType::STORAGE_BUFFER))
        | (1u << uint(GPUBufferType::ATOMIC_COUNTER))
        | (1u << uint(GPUBufferType::STAGING_BUFFER))
        | (1u << uint(GPUBufferType::INDIRECT_ARGS_BUFFER)),    // STORAGE_BUFFER
    
    (1u << uint(GPUBufferType::STORAGE_BUFFER))
        | (1u << uint(GPUBufferType::ATOMIC_COUNTER))
        | (1u << uint(GPUBufferType::STAGING_BUFFER))
        | (1u << uint(GPUBufferType::INDIRECT_ARGS_BUFFER)),    // STORAGE_BUFFER_DYNAMIC
    0,                                                          // IMAGE
    0,                                                          // IMAGE_STORAGE
    0,                                                          // SAMPLER
    (1u << uint(GPUBufferType::ACCELERATION_STRUCTURE_BUFFER))  // ACCELERATION_STRUCTURE
};

struct DescriptorSetLayoutElement
{
    DescriptorSetElementType    type = DescriptorSetElementType::UNSET;
    uint                        binding = -1; // has to be set
    uint                        count = 1; // Set to -1 for bindless

    bool IsBindless() const
        { return count == ~0u; }

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(type);
        hc.Add(binding);
        hc.Add(count);

        return hc;
    }
};

template <SizeType Sz>
static inline uint32 GetDescriptorSetElementTypeMask(FixedArray<DescriptorSetElementType, Sz> types)
{
    uint32 mask = 0;

    for (const auto &type : types) {
        mask |= 1 << uint32(type);
    }

    return mask;
}

namespace platform {

template <PlatformType PLATFORM>
class Device;
    
template <PlatformType PLATFORM>
class Instance;

template <PlatformType PLATFORM>
class DescriptorSet2;

template <PlatformType PLATFORM>
class GPUBuffer;

template <PlatformType PLATFORM>
class ImageView;

template <PlatformType PLATFORM>
class Sampler;

template <PlatformType PLATFORM>
class TopLevelAccelerationStructure;

template <class T>
struct DescriptorSetElementTypeInfo
{
    static_assert(resolution_failure<T>, "Implementation missing for this type");
};

template <>
struct DescriptorSetElementTypeInfo<GPUBuffer<Platform::VULKAN>>
{
    static constexpr uint32 mask = (1u << uint32(DescriptorSetElementType::UNIFORM_BUFFER))
        | (1u << uint32(DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC))
        | (1u << uint32(DescriptorSetElementType::STORAGE_BUFFER))
        | (1u << uint32(DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC));
};

template <>
struct DescriptorSetElementTypeInfo<ImageView<Platform::VULKAN>>
{
    static constexpr uint32 mask = (1u << uint32(DescriptorSetElementType::IMAGE))
        | (1u << uint32(DescriptorSetElementType::IMAGE_STORAGE));
};

template <>
struct DescriptorSetElementTypeInfo<Sampler<Platform::VULKAN>>
{
    static constexpr uint32 mask = (1u << uint32(DescriptorSetElementType::SAMPLER));
};

template <>
struct DescriptorSetElementTypeInfo<TopLevelAccelerationStructure<Platform::VULKAN>>
{
    static constexpr uint32 mask = (1u << uint32(DescriptorSetElementType::TLAS));
};

template <PlatformType PLATFORM>
class DescriptorSetLayout
{
public:
    DescriptorSetLayout(const DescriptorSetDeclaration &decl);
    DescriptorSetLayout(const DescriptorSetLayout &other)                   = default;
    DescriptorSetLayout &operator=(const DescriptorSetLayout &other)        = default;
    DescriptorSetLayout(DescriptorSetLayout &&other) noexcept               = default;
    DescriptorSetLayout &operator=(DescriptorSetLayout &&other) noexcept    = default;
    ~DescriptorSetLayout()                                                  = default;

    DescriptorSet2Ref<PLATFORM> CreateDescriptorSet() const;

    const ArrayMap<String, DescriptorSetLayoutElement> &GetElements() const
        { return m_elements; }

    void AddElement(const String &name, DescriptorSetElementType type, uint binding, uint count)
        { m_elements.Insert(name, DescriptorSetLayoutElement { type, binding, count }); }

    const DescriptorSetLayoutElement *GetElement(const String &name) const
    {
        const auto it = m_elements.Find(name);

        if (it == m_elements.End()) {
            return nullptr;
        }

        return &it->second;
    }

    HashCode GetHashCode() const
    {
        HashCode hc;

        for (const auto &it : m_elements) {
            hc.Add(it.first.GetHashCode());
            hc.Add(it.second.GetHashCode());
        }

        return hc;
    }

private:
    ArrayMap<String, DescriptorSetLayoutElement>    m_elements;
};

template <PlatformType PLATFORM>
struct DescriptorSetElement
{
    using ValueType = Variant<GPUBufferRef<PLATFORM>, ImageViewRef<PLATFORM>, SamplerRef<PLATFORM>, TLASRef<PLATFORM>>;

    FlatMap<uint, ValueType>    values;
    uint                        buffer_size = 0; // Used for dynamic uniform buffers
    Range<uint>                 dirty_range { };     

    HYP_FORCE_INLINE
    bool IsDirty() const
        { return bool(dirty_range); }
};

template <PlatformType PLATFORM>
class DescriptorSet2
{
public:
    DescriptorSet2(const DescriptorSetLayout<PLATFORM> &layout);
    DescriptorSet2(const DescriptorSet2 &other)                 = delete;
    DescriptorSet2 &operator=(const DescriptorSet2 &other)      = delete;
    DescriptorSet2(DescriptorSet2 &&other) noexcept             = delete;
    DescriptorSet2 &operator=(DescriptorSet2 &&other) noexcept  = delete;
    ~DescriptorSet2();

    const DescriptorSetLayout<PLATFORM> &GetLayout() const
        { return m_layout; }
    
    void SetElement(const String &name, const GPUBufferRef<PLATFORM> &ref);
    void SetElement(const String &name, uint index, const GPUBufferRef<PLATFORM> &ref);
    void SetElement(const String &name, uint index, uint buffer_size, const GPUBufferRef<PLATFORM> &ref);
    
    void SetElement(const String &name, const ImageViewRef<PLATFORM> &ref);
    void SetElement(const String &name, uint index, const ImageViewRef<PLATFORM> &ref);
    
    void SetElement(const String &name, const SamplerRef<PLATFORM> &ref);
    void SetElement(const String &name, uint index, const SamplerRef<PLATFORM> &ref);
    
    void SetElement(const String &name, const TLASRef<PLATFORM> &ref);
    void SetElement(const String &name, uint index, const TLASRef<PLATFORM> &ref);

private:
    DescriptorSetLayout<PLATFORM>                       m_layout;
    ArrayMap<String, DescriptorSetElement<PLATFORM>>    m_elements;
};

template <PlatformType PLATFORM>
class DescriptorSetManager
{
public:
};

} // namespace platform

} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererDescriptorSet2.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using DescriptorSet2 = platform::DescriptorSet2<Platform::CURRENT>;
using DescriptorSetLayout = platform::DescriptorSetLayout<Platform::CURRENT>;
using DescriptorSetManager = platform::DescriptorSetManager<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif