#ifndef GAMMA_CORRECT_H
#define GAMMA_CORRECT_H

#include "../post_shader.h"

namespace apex {
class GammaCorrectShader : public PostShader {
public:
    GammaCorrectShader::GammaCorrectShader(const ShaderProperties &properties);

    virtual void ApplyTransforms(const Matrix4 &transform, Camera *camera);
};
} // namespace apex

#endif