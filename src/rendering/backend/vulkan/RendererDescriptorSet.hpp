#ifndef HYPERION_RENDERER_BACKEND_VULKAN_DESCRIPTOR_SET_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_DESCRIPTOR_SET_HPP

#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/Platform.hpp>

#include <util/Defines.hpp>
#include <Constants.hpp>

#include <core/lib/Range.hpp>
#include <core/lib/FlatSet.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/FixedArray.hpp>
#include <core/lib/Queue.hpp>

#include <Types.hpp>

#include <vulkan/vulkan.h>

#include <vector>
#include <array>
#include <memory>
#include <unordered_map>
#include <map>
#include <queue>
#include <type_traits>

namespace hyperion {
namespace renderer {

namespace platform {

template <PlatformType PLATFORM>
class Device;

template <PlatformType PLATFORM>
class CommandBuffer;
    
template <PlatformType PLATFORM>
class ImageView;

template <PlatformType PLATFORM>
class Sampler;

template <PlatformType PLATFORM>
class Pipeline;

template <PlatformType PLATFORM>
class GraphicsPipeline;

template <PlatformType PLATFORM>
class ComputePipeline;

template <PlatformType PLATFORM>
class RaytracingPipeline;

} // namespace platform

using Device                = platform::Device<Platform::VULKAN>;
using CommandBuffer         = platform::CommandBuffer<Platform::VULKAN>;
using ImageView             = platform::ImageView<Platform::VULKAN>;
using Sampler               = platform::Sampler<Platform::VULKAN>;
using Pipeline              = platform::Pipeline<Platform::VULKAN>;
using GraphicsPipeline      = platform::GraphicsPipeline<Platform::VULKAN>;
using ComputePipeline       = platform::ComputePipeline<Platform::VULKAN>;
using RaytracingPipeline    = platform::RaytracingPipeline<Platform::VULKAN>;

class AccelerationStructure;
class DescriptorSet;
class DescriptorPool;

class Descriptor
{
    friend class DescriptorSet;
public:

    struct SubDescriptor
    {
        UInt element_index = ~0u; /* ~0 == just use index of item added */

        union {
            struct /* BufferData */ {
                const GPUBuffer *buffer;
                UInt range; /* if 0 then it is set to buffer->size */

                VkDescriptorBufferInfo buffer_info;
            };

            struct /* ImageData */ {
                const ImageView *image_view;
                const Sampler *sampler;

                VkDescriptorImageInfo image_info;
            };

            struct /* AccelerationStructureData */ {
                const AccelerationStructure *acceleration_structure;

                VkWriteDescriptorSetAccelerationStructureKHR acceleration_structure_info;
            };

            struct {
                uint64_t raw;
            };
        };

        bool valid = false; /* set internally to mark objects ready to be popped */

        HYP_DEF_STRUCT_COMPARE_EQL(SubDescriptor);

        bool operator<(const SubDescriptor &other) const
        {
            return std::tie(element_index, raw, valid) < std::tie(other.element_index, other.raw, other.valid);
        }
    };

    Descriptor(UInt binding, DescriptorType descriptor_type);
    Descriptor(const Descriptor &other) = delete;
    Descriptor &operator=(const Descriptor &other) = delete;
    ~Descriptor();

    UInt GetBinding() const
        { return m_binding; }

    void SetBinding(UInt binding)
        { m_binding = binding; }
                                          
    /* Sub descriptor --> ... uniform Thing { ... } things[5]; */
    auto &GetSubDescriptors()
        { return m_sub_descriptors; }

    /* Sub descriptor --> ... uniform Thing { ... } things[5]; */
    const auto &GetSubDescriptors() const
        { return m_sub_descriptors; }

    SubDescriptor &GetSubDescriptor(UInt index)
        { return m_sub_descriptors.At(index); }

    const SubDescriptor &GetSubDescriptor(UInt index) const
        { return m_sub_descriptors.At(index); }

    /*! \brief Add a sub-descriptor to this descriptor.
     *  Records that a sub-descriptor at the index has been changed,
     *  so you can call this after the descriptor has been initialized.
     * @param sub_descriptor An object containing buffer or image info about the sub descriptor to be added.
     * @returns index of descriptor
     */
    UInt SetSubDescriptor(SubDescriptor &&sub_descriptor);

    template <class Buffer>
    Descriptor *SetElementBuffer(UInt index, const GPUBuffer *buffer)
    {
        AssertThrowMsg(
            IsDescriptorTypeDynamicBuffer(m_descriptor_type),
            "Descriptor type must be a dynamic buffer to use this method"
        );

        SubDescriptor element;
        element.element_index = index;
        element.buffer = buffer;
        element.range = UInt32(sizeof(Buffer));

        SetSubDescriptor(std::move(element));

        return this;
    }

    template <class Buffer>
    Descriptor *SetElementBuffer(const GPUBuffer *buffer)
    {
        AssertThrowMsg(
            IsDescriptorTypeDynamicBuffer(m_descriptor_type),
            "Descriptor type must be a dynamic buffer to use this method"
        );
        
        SubDescriptor element;
        element.element_index = 0;
        element.buffer = buffer;
        element.range = UInt32(sizeof(Buffer));

        SetSubDescriptor(std::move(element));

        return this;
    }

    Descriptor *SetElementBuffer(UInt index, const GPUBuffer *buffer)
    {
        AssertThrowMsg(
            IsDescriptorTypeBuffer(m_descriptor_type) && !IsDescriptorTypeDynamicBuffer(m_descriptor_type),
            "Descriptor type must be a buffer (non-dynamic) to use this method"
        );

        SubDescriptor element;
        element.element_index = index;
        element.buffer = buffer;
        element.range = 0;

        SetSubDescriptor(std::move(element));

        return this;
    }

    Descriptor *SetElementBuffer(const GPUBuffer *buffer)
    {
        AssertThrowMsg(
            IsDescriptorTypeBuffer(m_descriptor_type) && !IsDescriptorTypeDynamicBuffer(m_descriptor_type),
            "Descriptor type must be a buffer (non-dynamic) to use this method"
        );

        SubDescriptor element;
        element.element_index = 0;
        element.buffer = buffer;
        element.range = 0;

        SetSubDescriptor(std::move(element));

        return this;
    }

    Descriptor *SetElementSRV(UInt index, const ImageView *image_view)
    {
        AssertThrowMsg(
            m_descriptor_type == DescriptorType::IMAGE,
            "SetElementSRV() requires descriptor of type IMAGE."
        );

        SubDescriptor element;
        element.element_index = index;
        element.image_view = image_view;

        SetSubDescriptor(std::move(element));

        return this;
    }

    Descriptor *SetElementUAV(UInt index, const ImageView *image_view)
    {
        AssertThrowMsg(
            m_descriptor_type == DescriptorType::IMAGE_STORAGE,
            "SetElementUAV() requires descriptor of type IMAGE_STORAGE."
        );

        SubDescriptor element;
        element.element_index = index;
        element.image_view = image_view;

        SetSubDescriptor(std::move(element));

        return this;
    }

    Descriptor *SetElementSampler(UInt index, const Sampler *sampler)
    {
        AssertThrowMsg(
            m_descriptor_type == DescriptorType::SAMPLER,
            "SetElementSampler() requires descriptor of type SAMPLER."
        );

        SubDescriptor element;
        element.element_index = index;
        element.sampler = sampler;

        SetSubDescriptor(std::move(element));

        return this;
    }

    Descriptor *SetElementImageSamplerCombined(UInt index, const ImageView *image_view, const Sampler *sampler)
    {
        AssertThrowMsg(
            m_descriptor_type == DescriptorType::IMAGE_SAMPLER,
            "SetElementImageSamplerCombined() requires descriptor of type IMAGE_SAMPLER."
        );

        SubDescriptor element;
        element.element_index = index;
        element.image_view = image_view;
        element.sampler = sampler;

        SetSubDescriptor(std::move(element));

        return this;
    }
    
    Descriptor *SetElementAccelerationStructure(UInt index, const AccelerationStructure *acceleration_structure)
    {
        AssertThrowMsg(
            m_descriptor_type == DescriptorType::ACCELERATION_STRUCTURE,
            "SetElementAccelerationStructure() requires descriptor of type ACCELERATION_STRUCTURE."
        );

        SubDescriptor element;
        element.element_index = index;
        element.acceleration_structure = acceleration_structure;

        SetSubDescriptor(std::move(element));

        return this;
    }

    /*! \brief Remove the sub-descriptor at the given index. */
    void RemoveSubDescriptor(UInt index);
    /*! \brief Mark a sub-descriptor as dirty */
    void MarkDirty(UInt sub_descriptor_index);

    void Create(
        Device *device,
        VkDescriptorSetLayoutBinding &binding,
        Array<VkWriteDescriptorSet> &writes
    );

protected:
    static VkDescriptorType ToVkDescriptorType(DescriptorType descriptor_type);

    void BuildUpdates(Device *device, Array<VkWriteDescriptorSet> &writes);
    void UpdateSubDescriptorBuffer(
        const SubDescriptor &sub_descriptor,
        VkDescriptorBufferInfo &out_buffer,
        VkDescriptorImageInfo &out_image,
        VkWriteDescriptorSetAccelerationStructureKHR &out_acceleration_structure
    ) const;

    Range<UInt> m_dirty_sub_descriptors;

    FlatMap<UInt, SubDescriptor> m_sub_descriptors;
    Array<UInt> m_sub_descriptor_update_indices;

    UInt m_binding;
    DescriptorType m_descriptor_type;

private:
    DescriptorSet *m_descriptor_set;
};

class DescriptorSet
{
    friend class Descriptor;
public:
    using Index = UInt;

    enum IndexNames : Index
    {
        DESCRIPTOR_SET_INDEX_UNUSED,         /* unused at the moment, pending removal */

        DESCRIPTOR_SET_INDEX_GLOBAL,         /* global, ideally bound once at beginning of frame */
        DESCRIPTOR_SET_INDEX_SCENE,          /* bound per scene / pass */
        DESCRIPTOR_SET_INDEX_OBJECT,         /* bound per each object */
        
        DESCRIPTOR_SET_INDEX_GLOBAL_FRAME_1,         
        DESCRIPTOR_SET_INDEX_SCENE_FRAME_1,  /* per scene - frame #2 (frames in flight) */
        DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1, /* per object - frame #2 (frames in flight) */

        DESCRIPTOR_SET_INDEX_BINDLESS,
        DESCRIPTOR_SET_INDEX_BINDLESS_FRAME_1,

        DESCRIPTOR_SET_INDEX_VOXELIZER,

        DESCRIPTOR_SET_INDEX_RAYTRACING,

        DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES,

        DESCRIPTOR_SET_INDEX_MAX
    };
    
    static constexpr Index global_buffer_mapping[] = {
        DESCRIPTOR_SET_INDEX_GLOBAL,
        DESCRIPTOR_SET_INDEX_GLOBAL_FRAME_1
    };
    
    static constexpr Index scene_buffer_mapping[]  = {
        DESCRIPTOR_SET_INDEX_SCENE,
        DESCRIPTOR_SET_INDEX_SCENE_FRAME_1
    };

    static constexpr Index object_buffer_mapping[] = {
        DESCRIPTOR_SET_INDEX_OBJECT,
        DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1
    };

    static constexpr Index bindless_textures_mapping[] = {
        DESCRIPTOR_SET_INDEX_BINDLESS,
        DESCRIPTOR_SET_INDEX_BINDLESS_FRAME_1
    };

    static constexpr UInt max_descriptor_sets                  = 5000;
    static constexpr UInt max_bindless_resources               = 4096;
    static constexpr UInt max_sub_descriptor_updates_per_frame = 16;
    static constexpr UInt max_bound_descriptor_sets            = 4; /* 0 = no cap */
    static constexpr UInt max_material_texture_samplers        = 16;

    static const std::map<Index, std::map<DescriptorKey, UInt>> mappings;
    static const FlatMap<Index, UInt> desired_indices;
    static Index GetBaseIndex(UInt index); // map index to the real index used (this is per-frame stuff)
    static Index GetPerFrameIndex(Index index, UInt frame_index);
    static Index GetPerFrameIndex(Index index, UInt instance_index, UInt frame_index);
    /*! Get the per-frame index of a descriptor set's /real/ index. Returns -1 if applicable to any. */
    static int GetFrameIndex(UInt real_index);
    static UInt GetDesiredIndex(Index index);

    /*! \brief Create a 'standalone' descriptor set.
        This is a newer way of creating them that will let us create descriptor sets that own their own
        resources such as layout. You would hold the DescriptorSet as a class member manually Create()/Destroy() it. */
    DescriptorSet();

    DescriptorSet(DescriptorSetDeclaration declaration);

    /*! \brief Create a descriptor set the older way. The descriptor set will be held in the DescriptorPool and managed
        indirectly by going through methods on DescriptorPool etc. */
    DescriptorSet(Index index, UInt real_index, Bool bindless);
    DescriptorSet(const DescriptorSet &other) = delete;
    DescriptorSet &operator=(const DescriptorSet &other) = delete;
    DescriptorSet(DescriptorSet &&other) noexcept;
    DescriptorSet &operator=(DescriptorSet &&other) noexcept = delete;
    ~DescriptorSet();

    DescriptorSetState GetState() const { return m_state; }
    Index GetIndex() const { return m_index; }
    UInt GetRealIndex() const { return m_real_index; }
    Bool IsBindless() const { return m_bindless; }
    Bool IsCreated() const { return m_is_created; }

    /* doesn't allocate a descriptor set, just a template for other material textures to follow. Creates a layout. */
    Bool IsTemplate() const { return GetRealIndex() == UInt(DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES); }

    template <class DescriptorType>
    Descriptor *AddDescriptor(DescriptorKey key)
        { return AddDescriptor<DescriptorType>(DescriptorKeyToIndex(key)); }

    template <class DescriptorType>
    Descriptor *AddDescriptor(UInt binding)
    {
        static_assert(std::is_base_of_v<Descriptor, DescriptorType>, "DescriptorType must be a derived class of Descriptor");

        auto it = m_descriptors.FindIf([binding](const auto &item)
        {
            return item->GetBinding() == binding;
        });

        AssertThrowMsg(it == m_descriptors.End(), "Descriptor with binding %u already exists", binding);

        m_descriptors.PushBack(std::make_unique<DescriptorType>(binding));
        m_descriptor_bindings.PushBack({ });

        return m_descriptors.Back().get();
    }

    Bool RemoveDescriptor(Descriptor *descriptor);
    Bool RemoveDescriptor(DescriptorKey key);
    Bool RemoveDescriptor(UInt binding);

    Bool HasDescriptorByName(Name name) const
        { return m_declaration.FindDescriptorDeclaration(name) != nullptr; }

    template <SizeType Count>
    Bool HasDescriptorByName(FixedArray<Name, Count> names) const
    {
        for (const auto item : names) {
            if (!m_declaration.FindDescriptorDeclaration(item)) {
                return false;
            }
        }

        return true;
    }

    Descriptor *GetDescriptor(DescriptorKey key) const;
    Descriptor *GetDescriptor(UInt binding) const;
    /*! \brief Get a Descriptor by name -- must have a DescriptorSetDeclaration */
    Descriptor *GetDescriptorByName(Name name) const;

    template <class DescriptorType>
    Descriptor *GetOrAddDescriptor(DescriptorKey key)
    {
        return GetOrAddDescriptor<DescriptorType>(DescriptorKeyToIndex(key));
    }

    template <class DescriptorType>
    Descriptor *GetOrAddDescriptor(UInt binding)
    {
        static_assert(std::is_base_of_v<Descriptor, DescriptorType>, "DescriptorType must be a derived class of Descriptor");

        if (auto *descriptor = GetDescriptor(binding)) {
            return descriptor;
        }

        return AddDescriptor<DescriptorType>(binding);
    }

    Array<std::unique_ptr<Descriptor>> &GetDescriptors() { return m_descriptors; }
    const Array<std::unique_ptr<Descriptor>> &GetDescriptors() const { return m_descriptors; }

    Result Create(Device *device, DescriptorPool *pool);
    Result Destroy(Device *device);

    void ApplyUpdates(Device *device);

    VkDescriptorSet m_set;
    VkDescriptorSetLayout m_layout;

private:
    /* Add descriptors from m_declaration */
    void AddDescriptors();

    UInt DescriptorKeyToIndex(DescriptorKey key) const;

    DescriptorSetDeclaration            m_declaration;

    DescriptorPool                      *m_descriptor_pool;
    Array<std::unique_ptr<Descriptor>>  m_descriptors;
    Array<VkDescriptorSetLayoutBinding> m_descriptor_bindings; /* one per each descriptor */
    Array<VkWriteDescriptorSet>         m_descriptor_writes; /* any number of per descriptor - reset after each update */
    DescriptorSetState                  m_state;
    Index                               m_index;
    UInt                                m_real_index;
    Bool                                m_bindless;
    Bool                                m_is_created;
    Bool                                m_is_standalone; // a descriptor set is 'standalone' if it is not created as part of the DescriptorPool.
                                                         // it it manages its own layout resource, as well.
};

struct DescriptorSetBinding {
    struct Declaration {
        DescriptorSet::Index set;
        UInt count = 1;
    } declaration;

    /* where we bind to in the shader program */
    struct Locations {
        DescriptorSet::Index binding; /* defaults to `set` */
    } locations;

    struct DynamicOffsets {
        std::vector<UInt> offsets;
    } offsets;

    DescriptorSetBinding()
        : declaration({
              .set = DescriptorSet::DESCRIPTOR_SET_INDEX_UNUSED,
              .count = DescriptorSet::max_descriptor_sets
          }),
          locations({
              .binding = DescriptorSet::DESCRIPTOR_SET_INDEX_UNUSED
          })
    {
    }

    DescriptorSetBinding(Declaration &&dec)
        : declaration(std::move(dec)),
          locations({})
    {
        locations.binding = declaration.set;

        if (declaration.count == 0) {
            declaration.count = DescriptorSet::DESCRIPTOR_SET_INDEX_MAX - declaration.set;
        }
    }

    DescriptorSetBinding(Declaration &&dec, Locations &&loc)
        : declaration(std::move(dec)),
          locations(std::move(loc))
    {
        if (declaration.count == 0) {
            declaration.count = DescriptorSet::DESCRIPTOR_SET_INDEX_MAX - declaration.set;
        }
    }

    DescriptorSetBinding(Declaration &&dec, Locations &&loc, DynamicOffsets &&offsets)
        : DescriptorSetBinding(std::move(dec), std::move(loc))
    {
        this->offsets = std::move(offsets);  // NOLINT(cppcoreguidelines-prefer-member-initializer)
    }
};

class DescriptorPool
{
    friend class DescriptorSet;

    Result AllocateDescriptorSet(Device *device, VkDescriptorSetLayout *layout, DescriptorSet *out);
    Result CreateDescriptorSetLayout(Device *device, UInt index, VkDescriptorSetLayoutCreateInfo *layout_create_info, VkDescriptorSetLayout *out);
    Result DestroyDescriptorSetLayout(Device *device, UInt index);
    VkDescriptorSetLayout GetDescriptorSetLayout(UInt index);

public:
    static const std::unordered_map<VkDescriptorType, UInt> items_per_set;

    DescriptorPool();
    DescriptorPool(const DescriptorPool &other) = delete;
    DescriptorPool &operator=(const DescriptorPool &other) = delete;
    ~DescriptorPool();
    
    VkDescriptorPool GetHandle() const { return m_descriptor_pool; }

    auto &GetDescriptorSets() { return m_descriptor_sets; }
    const auto &GetDescriptorSets() const { return m_descriptor_sets; }

    SizeType NumDescriptorSets() const { return m_descriptor_sets.Size(); }
    bool IsCreated() const { return m_is_created; }

    auto &GetDescriptorSetLayouts() { return m_descriptor_set_layouts; }
    const auto &GetDescriptorSetLayouts() const { return m_descriptor_set_layouts; }

    void SetDescriptorSetLayout(UInt index, VkDescriptorSetLayout layout);

    void AddDescriptorSet(
        Device *device,
        DescriptorSetRef descriptor_set,
        bool add_immediately = false
    );
    DescriptorSetRef GetDescriptorSet(DescriptorSet::Index index) const
        { return m_descriptor_sets[index]; }

    void RemoveDescriptorSet(Device *device, DescriptorSet *descriptor_set);
    void RemoveDescriptorSet(Device *device, UInt index);

    Result Create(Device *device);
    Result Destroy(Device *device);
    Result Bind(Device *device, CommandBuffer *cmd, GraphicsPipeline *pipeline, const DescriptorSetBinding &) const;
    Result Bind(Device *device, CommandBuffer *cmd, ComputePipeline *pipeline, const DescriptorSetBinding &) const;
    Result Bind(Device *device, CommandBuffer *cmd, RaytracingPipeline *pipeline, const DescriptorSetBinding &) const;

    Result CreateDescriptorSets(Device *device);
    Result DestroyPendingDescriptorSets(Device *device, UInt frame_index);
    Result AddPendingDescriptorSets(Device *device, UInt frame_index);
    Result UpdateDescriptorSets(Device *device, UInt frame_index);

private:
    Result DestroyDescriptorSet(Device *device, const DescriptorSetRef &descriptor_set);
    
    void BindDescriptorSets(
        Device *device,
        CommandBuffer *cmd,
        VkPipelineBindPoint bind_point,
        Pipeline *pipeline,
        const DescriptorSetBinding &binding
    ) const;

    Array<DescriptorSetRef>                                         m_descriptor_sets;
    FlatMap<UInt, VkDescriptorSetLayout>                            m_descriptor_set_layouts;
    VkDescriptorPool                                                m_descriptor_pool;

    FixedArray<Array<DescriptorSetRef>, max_frames_in_flight>       m_descriptor_sets_pending_addition;
    FixedArray<Queue<DescriptorSet::Index>, max_frames_in_flight>   m_descriptor_sets_pending_destruction;

    bool m_is_created;
};

} // namespace renderer
} // namespace hyperion

#endif
