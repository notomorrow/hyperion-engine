#ifndef SH_COMPUTE_SHADER_H
#define SH_COMPUTE_SHADER_H

#include "compute_shader.h"

namespace hyperion {
class SHComputeShader : public ComputeShader {
public:
    SHComputeShader(const ShaderProperties &properties);
    virtual ~SHComputeShader();

    DeclaredUniform::Id_t m_uniform_src_texture;
};
} // namespace hyperion

#endif