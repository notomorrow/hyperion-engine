#ifndef HYPERION_RENDERER_PIPELINE_H
#define HYPERION_RENDERER_PIPELINE_H

#include <vulkan/vulkan.h>

#include <cstdint>

namespace hyperion {
namespace renderer {

class Pipeline {
    friend class DescriptorPool;

public:
    struct PushConstantData {
        union {
            struct {  // NOLINT(clang-diagnostic-nested-anon-types)
                uint32_t grid_size,
                         count_mode;
            } voxelizer_data;

            struct {  // NOLINT(clang-diagnostic-nested-anon-types)
                uint32_t num_fragments,
                         voxel_grid_size,
                         mipmap_level;
            } octree_data;

            struct {  // NOLINT(clang-diagnostic-nested-anon-types)
                uint32_t x, y;
            } counter;

            struct {
                float    matrix[16];
                uint32_t time;
            } probe_data;
        };
    } push_constants;

    static_assert(sizeof(PushConstantData) <= 128);

    Pipeline();
    Pipeline(const Pipeline &other) = delete;
    Pipeline &operator=(const Pipeline &other) = delete;
    ~Pipeline();

protected:
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

} // namespace renderer
} // namespace hyperion

#endif