#ifndef SSAO_H
#define SSAO_H

#include "../post_shader.h"

namespace hyperion {

class SSAOShader : public PostShader {
public:
    SSAOShader(const ShaderProperties &properties);
    virtual ~SSAOShader() = default;

    virtual void ApplyTransforms(const Transform &transform, Camera *camera);

private:
    DeclaredUniform::Id_t m_uniform_inverse_view_proj,
        m_uniform_poisson_disk[16];
};

} // namespace hyperion

#endif
