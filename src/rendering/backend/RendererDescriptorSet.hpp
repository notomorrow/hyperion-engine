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

} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererDescriptorSet.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

/* Convenience descriptor classes */
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