#ifndef DEPTH_SHADER_H
#define DEPTH_SHADER_H

#include "../shader.h"

namespace apex {
class DepthShader : public Shader {
public:
    DepthShader(const ShaderProperties &properties);
    virtual ~DepthShader() = default;

    virtual void ApplyMaterial(const Material &mat);
    virtual void ApplyTransforms(const Transform &transform, Camera *camera);
};
} // namespace apex

#endif
