#ifndef FUR_SHADER_H
#define FUR_SHADER_H

#include "lighting_shader.h"

namespace apex {
class FurShader : public LightingShader {
public:
    FurShader(const ShaderProperties &properties);
    virtual ~FurShader() = default;

    virtual void ApplyMaterial(const Material &mat) override;
    virtual void ApplyTransforms(const Transform &transform, Camera *camera) override;
};
} // namespace apex

#endif
