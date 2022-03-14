#ifndef HYPERION_V2_COMPUTE_SHADER_H
#define HYPERION_V2_COMPUTE_SHADER_H

#include "shader.h"
#include "pipeline.h"

#include <memory>

namespace hyperion::v2 {

using renderer::CommandBuffer;

class ComputeShader : public Shader {
public:
    explicit ComputeShader(const SpirvObject &spirv_object);
    ComputeShader(const ComputeShader &) = delete;
    ComputeShader &operator=(const ComputeShader &) = delete;
    ~ComputeShader();

    void Create(Engine *engine);
    void Destroy(Engine *engine);
    void Dispatch(Engine *engine);

private:
    Pipeline::ID m_pipeline_id;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_COMPUTE_SHADER_H

