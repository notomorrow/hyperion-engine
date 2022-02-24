#ifndef BLUR_COMPUTE_SHADER_H
#define BLUR_COMPUTE_SHADER_H

#include "compute_shader.h"

namespace hyperion {
class BlurComputeShader : public ComputeShader {
public:
    BlurComputeShader(const ShaderProperties &properties);
    virtual ~BlurComputeShader();

    DeclaredUniform::Id_t m_uniform_src_texture,
        m_uniform_src_mip_level;
};
} // namespace hyperion

#endif