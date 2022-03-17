#ifndef HYPERION_V2_COMPUTE_H
#define HYPERION_V2_COMPUTE_H

#include "shader.h"
#include "pipeline.h"

#include <rendering/backend/renderer_command_buffer.h>
#include <rendering/backend/renderer_compute_pipeline.h>

#include <memory>

namespace hyperion::v2 {

using renderer::CommandBuffer;

class ComputePipeline : public EngineComponent<renderer::ComputePipeline> {
public:
    explicit ComputePipeline(Shader::ID shader_id);
    ComputePipeline(const ComputePipeline &) = delete;
    ComputePipeline &operator=(const ComputePipeline &) = delete;
    ~ComputePipeline();

    void Create(Engine *engine);
    void Destroy(Engine *engine);
    void Dispatch(Engine *engine, CommandBuffer *command_buffer, size_t num_groups_x, size_t num_groups_y, size_t num_groups_z);

private:
    Shader::ID m_shader_id;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_COMPUTE_H

