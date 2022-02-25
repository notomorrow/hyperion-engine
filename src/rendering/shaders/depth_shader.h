#ifndef DEPTH_SHADER_H
#define DEPTH_SHADER_H

#include "../shader.h"

namespace hyperion {
class DepthShader : public Shader {
public:
    DepthShader(const ShaderProperties &properties);
    virtual ~DepthShader() = default;

    virtual void ApplyMaterial(const Material &mat);
    virtual void ApplyTransforms(const Transform &transform, Camera *camera);

private:
    DeclaredUniform::Id_t m_uniform_camera_position;
};
} // namespace hyperion

#endif
