#ifndef SSAO_H
#define SSAO_H

#include "../post_shader.h"

namespace apex {

class SSAOShader : public PostShader {
public:
    SSAOShader(const ShaderProperties &properties);
    virtual ~SSAOShader() = default;

    virtual void ApplyTransforms(const Matrix4 &transform, Camera *camera);
};

} // namespace apex

#endif
