#ifndef VEGETATION_SHADER_H
#define VEGETATION_SHADER_H

#include "lighting_shader.h"

namespace hyperion {
class VegetationShader : public LightingShader {
public:
    VegetationShader(const ShaderProperties &properties);
    virtual ~VegetationShader() = default;

    virtual void ApplyMaterial(const Material &mat);
    virtual void ApplyTransforms(const Transform &transform, Camera *camera);
};
} // namespace hyperion

#endif
