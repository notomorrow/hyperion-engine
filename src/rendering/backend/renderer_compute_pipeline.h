#ifndef HYPERION_RENDERER_COMPUTE_PIPELINE_H
#define HYPERION_RENDERER_COMPUTE_PIPELINE_H

#include "renderer_pipeline.h"
#include "renderer_device.h"
#include "renderer_swapchain.h"
#include "renderer_buffer.h"
#include "renderer_shader.h"
#include "renderer_descriptor_set.h"

namespace hyperion {
namespace renderer {
class ComputePipeline : public Pipeline {
public:
    ComputePipeline();
    ComputePipeline(const ComputePipeline &other) = delete;
    ComputePipeline &operator=(const ComputePipeline &other) = delete;
    ~ComputePipeline();

    Result Create(Device *device, ShaderProgram *shader, DescriptorPool *pool);
    Result Destroy(Device *device);

    void Bind(CommandBuffer *command_buffer) const;
    void Bind(CommandBuffer *command_buffer, const PushConstantData &push_constant_data);
    void Dispatch(CommandBuffer *command_buffer, Extent3D group_size) const;
    void DispatchIndirect(CommandBuffer *command_buffer,
        const IndirectBuffer *indirect,
        size_t offset = 0) const;
};

} // namespace renderer
}; // namespace hyperion

#endif //HYPERION_RENDERER_COMPUTE_PIPELINE_H
