#ifndef LIGHTING_SHADER_H
#define LIGHTING_SHADER_H

#include "../shader.h"

namespace hyperion {
class LightingShader : public Shader {
public:
    LightingShader(const ShaderProperties &properties);
    virtual ~LightingShader() = default;

    virtual void ApplyMaterial(const Material &mat);
    virtual void ApplyTransforms(const Transform &transform, Camera *camera);
};
} // namespace hyperion

#endif
