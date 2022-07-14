#ifndef HYPERION_RENDERER_COMPUTE_PIPELINE_H
#define HYPERION_RENDERER_COMPUTE_PIPELINE_H

#include "RendererPipeline.hpp"
#include "RendererDevice.hpp"
#include "RendererSwapchain.hpp"
#include "RendererBuffer.hpp"
#include "RendererShader.hpp"
#include "RendererDescriptorSet.hpp"

namespace hyperion {
namespace renderer {
class ComputePipeline : public Pipeline {
public:
    ComputePipeline();
    /*! \brief Construct a pipeline using the given \ref used_descriptor_set as the descriptor sets to be
        used with this pipeline.  */
    ComputePipeline(const DynArray<const DescriptorSet *> &used_descriptor_sets);
    ComputePipeline(const ComputePipeline &other) = delete;
    ComputePipeline &operator=(const ComputePipeline &other) = delete;
    ~ComputePipeline();

    Result Create(Device *device, ShaderProgram *shader, DescriptorPool *pool);
    Result Destroy(Device *device);

    void Bind(CommandBuffer *command_buffer) const;
    void Bind(CommandBuffer *command_buffer, const PushConstantData &push_constant_data);
    void Dispatch(CommandBuffer *command_buffer, Extent3D group_size) const;
    void DispatchIndirect(
        CommandBuffer *command_buffer,
        const IndirectBuffer *indirect,
        size_t offset = 0
    ) const;
};

} // namespace renderer
}; // namespace hyperion

#endif //HYPERION_RENDERER_COMPUTE_PIPELINE_H
