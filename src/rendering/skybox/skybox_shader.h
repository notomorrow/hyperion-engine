#ifndef SKYBOX_SHADER_H
#define SKYBOX_SHADER_H

#include "../shader.h"

namespace hyperion {
class SkyboxShader : public Shader {
public:
    SkyboxShader(const ShaderProperties &properties);
    virtual ~SkyboxShader() = default;

    virtual void ApplyMaterial(const Material &mat);
    virtual void ApplyTransforms(const Transform &transform, Camera *camera);

private:
    DeclaredUniform::Id_t m_uniform_camera_position;
};
} // namespace hyperion

#endif
