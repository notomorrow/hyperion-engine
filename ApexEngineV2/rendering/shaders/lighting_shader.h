#ifndef LIGHTING_SHADER_H
#define LIGHTING_SHADER_H

#include "../shader.h"

namespace apex {
class LightingShader : public Shader {
public:
    LightingShader(const ShaderProperties &properties);

    virtual void ApplyMaterial(const Material &mat);
    virtual void ApplyTransforms(const Matrix4 &transform, Camera *camera);
};
}

#endif