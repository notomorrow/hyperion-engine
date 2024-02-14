#ifndef HYPERION_RENDERER_BACKEND_VULKAN_PIPELINE_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_PIPELINE_HPP

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/Optional.hpp>

#include <math/Vector4.hpp>
#include <Types.hpp>

#include <vulkan/vulkan.h>

#include <cstdint>

namespace hyperion {
namespace renderer {
    
class DescriptorPool;

namespace platform {

template <PlatformType PLATFORM>
class Device;

template <PlatformType PLATFORM>
class ShaderProgram;

template <>
class Pipeline<Platform::VULKAN>
{
    friend class DescriptorPool;

public:
    struct alignas(128) PushConstantData
    {
        union
        {
            struct  // NOLINT(clang-diagnostic-nested-anon-types)
            {
                uint32 grid_size,
                    count_mode;
            } voxelizer_data;

            struct // NOLINT(clang-diagnostic-nested-anon-types)
            {
                uint32 num_fragments,
                    voxel_grid_size,
                    mipmap_level;
            } octree_data;

            struct // NOLINT(clang-diagnostic-nested-anon-types)
            {
                uint32 x, y;
            } counter;

            struct // NOLINT(clang-diagnostic-nested-anon-types)
            {
                float matrix[16];
                uint32 time;
            } probe_data;

            struct // NOLINT(clang-diagnostic-nested-anon-types)
            {
                uint32 extent[4];
                float aabb_max[4];
                float aabb_min[4];
            } vct_data;
            
            struct // NOLINT(clang-diagnostic-nested-anon-types)
            {
                uint32 current_effect_index_stage; // 31bits for index, 1 bit for stage
            } post_fx_data;

            struct // NOLINT(clang-diagnostic-nested-anon-types)
            {
                uint32 index;
            } render_component_data;

            struct // NOLINT(clang-diagnostic-nested-anon-types)
            {
                uint32 width, height;
                float ray_step,
                    num_iterations,
                    max_ray_distance,
                    distance_bias,
                    offset,
                    eye_fade_start,
                    eye_fade_end,
                    screen_edge_fade_start,
                    screen_edge_fade_end;
            } ssr_data;

            struct // NOLINT(clang-diagnostic-nested-anon-types)
            {
                uint32 flags;
            } deferred_data;

            struct // NOLINT(clang-diagnostic-nested-anon-types)
            {
                ShaderVec2<uint32> mip_dimensions;
                ShaderVec2<uint32> prev_mip_dimensions;
                uint32 mip_level;
            } depth_pyramid_data;
            
            struct // NOLINT(clang-diagnostic-nested-anon-types)
            {
                uint32 batch_offset;
                uint32 num_instances;
                uint32 scene_id;
                ShaderVec2<uint32> depth_pyramid_dimensions;
            } object_visibility_data;

            struct // NOLINT(clang-diagnostic-nested-anon-types)
            {
                ShaderVec4<uint32> mip_dimensions;
                ShaderVec4<uint32> prev_mip_dimensions;
                uint32 mip_level;
            } voxel_mip_data;

            struct // NOLINT(clang-diagnostic-nested-anon-types)
            {
                ShaderVec2<uint32> image_dimensions;
            } blur_shadow_map_data;

            struct // NOLINT(clang-diagnostic-nested-anon-types)
            {
                ShaderVec2<uint32> image_dimensions;
            } deferred_combine_data;

            struct // NOLINT(clang-diagnostic-nested-anon-types)
            {
                ShaderVec4<float32> origin;
                float32 spawn_radius;
                float32 randomness;
                float32 avg_lifespan;
                uint32 max_particles;
                float32 max_particles_sqrt;
                float32 delta_time;
                uint32 global_counter;
            } particle_spawner_data;
        };
    } push_constants;

    static_assert(sizeof(PushConstantData) <= 128);

    Pipeline();
    Pipeline(ShaderProgramRef<Platform::VULKAN> shader);
    /*! \brief Construct a pipeline using the given \ref used_descriptor_set as the descriptor sets to be
        used with this pipeline.  */
    Pipeline(const Array<DescriptorSetRef> &used_descriptor_sets);
    Pipeline(ShaderProgramRef<Platform::VULKAN> shader, const Array<DescriptorSetRef> &used_descriptor_sets);

    // new constructor
    Pipeline(ShaderProgramRef<Platform::VULKAN> shader, const Array<DescriptorSet2Ref<Platform::VULKAN>> &used_descriptor_sets);

    Pipeline(const Pipeline &other) = delete;
    Pipeline &operator=(const Pipeline &other) = delete;
    ~Pipeline();

    bool HasCustomDescriptorSets() const
        { return m_has_custom_descriptor_sets; }

    const Optional<Array<DescriptorSetRef>> &GetUsedDescriptorSets() const
        { return m_used_descriptor_sets; }

    void SetUsedDescriptorSets(const Array<DescriptorSetRef> &used_descriptor_sets)
    {
        if (used_descriptor_sets.Any()) {
            m_used_descriptor_sets = used_descriptor_sets;
            m_has_custom_descriptor_sets = true;
        } else {
            m_used_descriptor_sets.Unset();
            m_has_custom_descriptor_sets = false;
        }
    }

    void SetPushConstants(const PushConstantData &push_constants)
        { this->push_constants = push_constants; }
    
    void SetPushConstants(const void *data, SizeType size);

    VkPipelineLayout layout;

protected:
    void AssignDefaultDescriptorSets(DescriptorPool *descriptor_pool);

    // For DescriptorSet2
    Array<VkDescriptorSetLayout> GetDescriptorSetLayouts() const;

    std::vector<VkDescriptorSetLayout> GetDescriptorSetLayouts(Device<Platform::VULKAN> *device, DescriptorPool *descriptor_pool);

    ShaderProgramRef<Platform::VULKAN>                      m_shader_program;

    bool                                                    m_has_custom_descriptor_sets;
    Optional<Array<DescriptorSetRef>>                       m_used_descriptor_sets { };
    Optional<Array<DescriptorSet2Ref<Platform::VULKAN>>>    m_used_descriptor_sets2 { };

    VkPipeline                                              pipeline;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif
