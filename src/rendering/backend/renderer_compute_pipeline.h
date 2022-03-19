//
// Created by emd22 on 2022-02-20.
//

#ifndef HYPERION_RENDERER_COMPUTE_PIPELINE_H
#define HYPERION_RENDERER_COMPUTE_PIPELINE_H

#include <vulkan/vulkan.h>

#include "renderer_device.h"
#include "renderer_swapchain.h"
#include "renderer_buffer.h"
#include "renderer_shader.h"
#include "renderer_render_pass.h"
#include "renderer_descriptor_pool.h"
#include "renderer_descriptor_set.h"
#include "renderer_descriptor.h"
#include "renderer_helpers.h"

#include "../../hash_code.h"

namespace hyperion {
namespace renderer {
class ComputePipeline {
public:
    ComputePipeline();
    ComputePipeline(const ComputePipeline &other) = delete;
    ComputePipeline &operator=(const ComputePipeline &other) = delete;
    ~ComputePipeline();

    Result Create(Device *device, ShaderProgram *shader, DescriptorPool *pool);
    Result Destroy(Device *device);

    void Bind(VkCommandBuffer cmd) const;
    void Dispatch(VkCommandBuffer cmd, size_t num_groups_x, size_t num_groups_y, size_t num_groups_z) const;

    VkPipeline pipeline;
    VkPipelineLayout layout;

    struct PushConstants {
        uint32_t counter_x;
        uint32_t counter_y;
    } push_constants;
};

} // namespace renderer
}; // namespace hyperion

#endif //HYPERION_RENDERER_COMPUTE_PIPELINE_H
