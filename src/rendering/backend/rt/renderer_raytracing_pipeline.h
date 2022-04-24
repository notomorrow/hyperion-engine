#ifndef HYPERION_RENDERER_RAYTRACING_PIPELINE_H
#define HYPERION_RENDERER_RAYTRACING_PIPELINE_H

#include "../renderer_pipeline.h"
#include "../renderer_device.h"
#include "../renderer_buffer.h"
#include "../renderer_shader.h"
#include "../renderer_descriptor_set.h"

#include <memory>

namespace hyperion {
namespace renderer {

class RaytracingPipeline : public Pipeline {
public:
    RaytracingPipeline(std::unique_ptr<ShaderProgram> &&shader_program);
    RaytracingPipeline(const RaytracingPipeline &other) = delete;
    RaytracingPipeline &operator=(const RaytracingPipeline &other) = delete;
    ~RaytracingPipeline();

    Result Create(Device *device, DescriptorPool *pool);
    Result Destroy(Device *device);

    void Bind(CommandBuffer *command_buffer);

private:
    Result CreateShaderBindingTables(Device *device);

    Result CreateShaderBindingTable(Device *device,
        uint32_t num_shaders,
        std::unique_ptr<ShaderBindingTableBuffer> &out_buffer);

    std::unique_ptr<ShaderProgram> m_shader_program;
    std::vector<std::unique_ptr<ShaderBindingTableBuffer>> m_shader_binding_table_buffers;
};

} // namespace renderer
}; // namespace hyperion

#endif //HYPERION_RENDERER_RAYTRACING_PIPELINE_H
