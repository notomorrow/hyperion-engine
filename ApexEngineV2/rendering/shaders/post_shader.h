#ifndef POST_SHADER_H
#define POST_SHADER_H

#include "../shader.h"

namespace hyperion {
class PostShader : public Shader {
public:
    PostShader(const ShaderProperties &properties);
    virtual ~PostShader() = default;

    virtual void ApplyMaterial(const Material &mat) override;
    virtual void ApplyTransforms(const Transform &transform, Camera *camera) = 0;
};
} // namespace hyperion

#endif
