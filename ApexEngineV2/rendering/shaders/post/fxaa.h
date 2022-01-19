#ifndef FXAA_H
#define FXAA_H

#include "../post_shader.h"

namespace apex {

class FXAAShader : public PostShader {
public:
    FXAAShader(const ShaderProperties &properties);
    virtual ~FXAAShader() = default;

    virtual void ApplyTransforms(const Transform &transform, Camera *camera);
};

} // namespace apex

#endif
