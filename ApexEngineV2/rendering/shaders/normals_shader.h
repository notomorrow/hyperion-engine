#ifndef NORMALS_SHADER_H
#define NORMALS_SHADER_H

#include "../shader.h"

namespace apex {
class NormalsShader : public Shader {
public:
    NormalsShader(const ShaderProperties &properties);
    virtual ~NormalsShader() = default;

    virtual void ApplyMaterial(const Material &mat);
    virtual void ApplyTransforms(const Transform &transform, Camera *camera);
};
} // namespace apex

#endif
