#ifndef HYPERION_V2_BACKEND_RENDERER_DESCRIPTOR_SET_HPP
#define HYPERION_V2_BACKEND_RENDERER_DESCRIPTOR_SET_HPP

#include <core/Name.hpp>
#include <core/lib/Optional.hpp>
#include <core/lib/RefCountedPtr.hpp>
#include <core/lib/Mutex.hpp>
#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <Types.hpp>
#include <util/Defines.hpp>


namespace hyperion {
namespace renderer {

namespace platform {

template <PlatformType PLATFORM>
class Device;
    
template <PlatformType PLATFORM>
class Instance;

} // namespace platform

using Device    = platform::Device<Platform::CURRENT>;
using Instance  = platform::Instance<Platform::CURRENT>;

class DescriptorSet;
class DescriptorPool;

enum class DescriptorSetState
{
    DESCRIPTOR_CLEAN = 0,
    DESCRIPTOR_DIRTY = 1
};

enum class DescriptorType
{
    UNSET,
    UNIFORM_BUFFER,
    UNIFORM_BUFFER_DYNAMIC,
    STORAGE_BUFFER,
    STORAGE_BUFFER_DYNAMIC,
    IMAGE,
    SAMPLER,
    IMAGE_SAMPLER,
    IMAGE_STORAGE,
    ACCELERATION_STRUCTURE,
    MAX
};

enum class DescriptorKey
{
    UNUSED = 0,

    GBUFFER_TEXTURES,
    GBUFFER_DEPTH,
    GBUFFER_MIP_CHAIN,
    GBUFFER_DEPTH_SAMPLER,
    GBUFFER_SAMPLER,
    DEFERRED_RESULT,
    POST_FX_PRE_STACK,
    POST_FX_POST_STACK,
    POST_FX_UNIFORMS,
    SSR_UV_IMAGE,
    SSR_SAMPLE_IMAGE,
    SSR_UV_TEXTURE,
    SSR_SAMPLE_TEXTURE,
    SSR_FINAL_TEXTURE,
    VOXEL_IMAGE,
    DEPTH_PYRAMID_RESULT,
    SVO_BUFFER,
    SSR_RESULT,
    SSAO_GI_RESULT,
    UI_TEXTURE,
    MOTION_VECTORS_RESULT,
    RT_RADIANCE_RESULT,
    RT_PROBE_UNIFORMS,
    RT_IRRADIANCE_GRID,
    RT_DEPTH_GRID,
    TEMPORAL_AA_RESULT,
    IMMEDIATE_DRAWS,
    DEFERRED_LIGHTING_AMBIENT,
    DEFERRED_LIGHTING_DIRECT,
    DEFERRED_IRRADIANCE_ACCUM,
    DEFERRED_RADIANCE,
    DEFERRED_REFLECTION_PROBE,
    SH_GRID_BUFFER,
    LIGHT_FIELD_COLOR_BUFFER,
    LIGHT_FIELD_NORMALS_BUFFER,
    LIGHT_FIELD_DEPTH_BUFFER,
    LIGHT_FIELD_DEPTH_BUFFER_LOWRES,
    LIGHT_FIELD_IRRADIANCE_BUFFER,
    LIGHT_FIELD_FILTERED_DISTANCE_BUFFER,
    VOXEL_GRID_IMAGE,
    VCT_VOXEL_UAV,
    VCT_VOXEL_UNIFORMS,
    VCT_SVO_BUFFER,
    VCT_SVO_FRAGMENT_LIST,
    BLUE_NOISE_BUFFER,
    DOF_BLUR_HOR,
    DOF_BLUR_VERT,
    DOF_BLUR_BLENDED,
    FINAL_OUTPUT,

    SCENE_BUFFER,
    LIGHTS_BUFFER,
    ENV_GRID_BUFFER,
    CURRENT_ENV_PROBE,
    CAMERA_BUFFER,
    SHADOW_MAPS,
    SHADOW_MATRICES,
    ENVIRONMENT_MAPS,
    ENV_PROBE_TEXTURES,
    ENV_PROBES,
    POINT_SHADOW_MAPS,

    MATERIAL_BUFFER,
    OBJECT_BUFFER,
    SKELETON_BUFFER,
    ENTITY_INSTANCES,

    SAMPLER,
    TEXTURES

    // ... 

};

static inline bool IsDescriptorTypeBuffer(DescriptorType type)
{
    return type == DescriptorType::UNIFORM_BUFFER
        || type == DescriptorType::UNIFORM_BUFFER_DYNAMIC
        || type == DescriptorType::STORAGE_BUFFER
        || type == DescriptorType::STORAGE_BUFFER_DYNAMIC;
}

static inline bool IsDescriptorTypeDynamicBuffer(DescriptorType type)
{
    return type == DescriptorType::UNIFORM_BUFFER_DYNAMIC
        || type == DescriptorType::STORAGE_BUFFER_DYNAMIC;
}

/* Convenience descriptor classes */

enum DescriptorSlot : uint32
{
    DESCRIPTOR_SLOT_NONE,
    DESCRIPTOR_SLOT_SRV,
    DESCRIPTOR_SLOT_UAV,
    DESCRIPTOR_SLOT_CBUFF,
    DESCRIPTOR_SLOT_SSBO,
    DESCRIPTOR_SLOT_ACCELERATION_STRUCTURE,
    DESCRIPTOR_SLOT_SAMPLER,
    DESCRIPTOR_SLOT_MAX
};

struct DescriptorDeclaration
{
    DescriptorSlot  slot = DESCRIPTOR_SLOT_NONE;
    uint            index = ~0u;
    String          name;
    uint            count = 1;
    bool            is_dynamic = false;
};

struct DescriptorSetDeclaration
{
    uint set_index = ~0u;
    Name name = Name::invalid;
    FixedArray<
        Array<DescriptorDeclaration>,
        DESCRIPTOR_SLOT_MAX
    > slots = { };

    DescriptorSetDeclaration()                                                      = default;

    DescriptorSetDeclaration(uint set_index, Name name)
        : set_index(set_index),
          name(name)
    {
    }

    DescriptorSetDeclaration(const DescriptorSetDeclaration &other)                 = default;
    DescriptorSetDeclaration &operator=(const DescriptorSetDeclaration &other)      = default;
    DescriptorSetDeclaration(DescriptorSetDeclaration &&other) noexcept             = default;
    DescriptorSetDeclaration &operator=(DescriptorSetDeclaration &&other) noexcept  = default;
    ~DescriptorSetDeclaration()                                                     = default;

    Array<DescriptorDeclaration> &GetSlot(DescriptorSlot slot)
    {
        AssertThrow(slot < DESCRIPTOR_SLOT_MAX && slot > DESCRIPTOR_SLOT_NONE);

        return slots[uint(slot) - 1];
    }

    const Array<DescriptorDeclaration> &GetSlot(DescriptorSlot slot) const
    {
        AssertThrow(slot < DESCRIPTOR_SLOT_MAX && slot > DESCRIPTOR_SLOT_NONE);

        return slots[uint(slot) - 1];
    }

    void AddDescriptor(DescriptorSlot slot, const String &name, bool is_dynamic = false)
    {
        DebugLog(LogType::Debug, "Add descriptor to slot %u with name %s (%u), dynamic: %d\n", slot, name.Data(), name.GetHashCode().Value(), is_dynamic);

        AssertThrow(slot != DESCRIPTOR_SLOT_NONE && slot < DESCRIPTOR_SLOT_MAX);

        slots[uint(slot) - 1].PushBack(DescriptorDeclaration {
            .slot       = slot,
            .index      = uint(slots[uint(slot) - 1].Size()),
            .name       = name,
            .is_dynamic = is_dynamic
        });
    }

    /*! \brief Calculate a flat index for a Descriptor that is part of this set.
        Returns -1 if not found */
    uint CalculateFlatIndex(DescriptorSlot slot, const String &name) const;

    DescriptorDeclaration *FindDescriptorDeclaration(const String &name) const;
};

class DescriptorTable
{
public:

    DescriptorSetDeclaration *FindDescriptorSetDeclaration(Name name) const;
    DescriptorSetDeclaration *AddDescriptorSet(DescriptorSetDeclaration descriptor_set);

    Array<DescriptorSetDeclaration> &GetDescriptorSetDeclarations()
        { return declarations; }

    const Array<DescriptorSetDeclaration> &GetDescriptorSetDeclarations() const
        { return declarations; }

private:
    Array<DescriptorSetDeclaration> declarations;

public:

    struct DeclareSet
    {
        DeclareSet(DescriptorTable *table, uint set_index, Name name)
        {
            AssertThrow(table != nullptr);

            if (table->declarations.Size() <= set_index) {
                table->declarations.Resize(set_index + 1);
            }

            DescriptorSetDeclaration decl;
            decl.set_index = set_index;
            decl.name = name;
            table->declarations[set_index] = std::move(decl);
        }
    };

    struct DeclareDescriptor
    {
        DeclareDescriptor(DescriptorTable *table, Name set_name, DescriptorSlot slot_type, const String &descriptor_name, uint count = 1)
        {
            AssertThrow(table != nullptr);

            uint set_index = ~0u;

            for (uint i = 0; i < table->declarations.Size(); ++i) {
                if (table->declarations[i].name == set_name) {
                    set_index = i;
                    break;
                }
            }

            AssertThrowMsg(set_index != ~0u, "Descriptor set %s not found", set_name.LookupString());

            DescriptorSetDeclaration &decl = table->declarations[set_index];
            AssertThrow(decl.set_index == set_index);
            AssertThrow(slot_type > 0 && slot_type < decl.slots.Size());

            const uint slot_index = decl.slots[uint(slot_type) - 1].Size();

            DescriptorDeclaration descriptor_decl;
            descriptor_decl.index = slot_index;
            descriptor_decl.slot = slot_type;
            descriptor_decl.name = descriptor_name;
            descriptor_decl.count = count;
            decl.slots[uint(slot_type) - 1].PushBack(descriptor_decl);
        }
    };
};

extern DescriptorTable *g_static_descriptor_table;

// #define HYP_DESCRIPTOR_SETS_DECLARE
// #define HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE g_static_descriptor_table
// #include <rendering/inl/DescriptorSets.inl>
// #undef HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE
// #undef HYP_DESCRIPTOR_SETS_DECLARE

} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererDescriptorSet.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

#define HYP_DEFINE_DESCRIPTOR(class_name, descriptor_type) \
    class class_name : public Descriptor { \
    public: \
        class_name(\
            uint binding \
        ) : Descriptor(binding, descriptor_type) {} \
    }

HYP_DEFINE_DESCRIPTOR(UniformBufferDescriptor,        DescriptorType::UNIFORM_BUFFER);
HYP_DEFINE_DESCRIPTOR(DynamicUniformBufferDescriptor, DescriptorType::UNIFORM_BUFFER_DYNAMIC);
HYP_DEFINE_DESCRIPTOR(StorageBufferDescriptor,        DescriptorType::STORAGE_BUFFER);
HYP_DEFINE_DESCRIPTOR(DynamicStorageBufferDescriptor, DescriptorType::STORAGE_BUFFER_DYNAMIC);
HYP_DEFINE_DESCRIPTOR(ImageDescriptor,                DescriptorType::IMAGE);
HYP_DEFINE_DESCRIPTOR(SamplerDescriptor,              DescriptorType::SAMPLER);
HYP_DEFINE_DESCRIPTOR(ImageSamplerDescriptor,         DescriptorType::IMAGE_SAMPLER);
HYP_DEFINE_DESCRIPTOR(StorageImageDescriptor,         DescriptorType::IMAGE_STORAGE);
HYP_DEFINE_DESCRIPTOR(TlasDescriptor,                 DescriptorType::ACCELERATION_STRUCTURE);

#undef HYP_DEFINE_DESCRIPTOR

} // namespace renderer
} // namespace hyperion

#endif