#ifndef UI_OBJECT_SHADER_H
#define UI_OBJECT_SHADER_H

#include "../../shader.h"

namespace hyperion {
class UIObjectShader : public Shader {
public:
    UIObjectShader(const ShaderProperties &properties);
    virtual ~UIObjectShader() = default;

    virtual void ApplyMaterial(const Material &mat) override;
    virtual void ApplyTransforms(const Transform &transform, Camera *camera) override;
};
} // namespace hyperion

#endif
