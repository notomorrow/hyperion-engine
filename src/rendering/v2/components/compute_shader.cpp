#include "compute_shader.h"
#include "../engine.h"

namespace hyperion::v2 {
ComputeShader::ComputeShader(const SpirvObject &spirv_object)
    : Shader({ spirv_object })
{
}

ComputeShader::~ComputeShader() = default;

void ComputeShader::Create(Engine *engine)
{
}

void ComputeShader::Destroy(Engine *engine)
{
}
} // namespace hyperion