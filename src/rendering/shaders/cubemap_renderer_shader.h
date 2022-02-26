#ifndef CUBEMAP_RENDERER_SHADER_H
#define CUBEMAP_RENDERER_SHADER_H

#include <array>

#include "../shader.h"
#include "../../math/matrix4.h"

namespace hyperion {
class CubemapRendererShader : public Shader {
public:
    CubemapRendererShader(const ShaderProperties &properties);
    virtual ~CubemapRendererShader() = default;

    virtual void ApplyMaterial(const Material &mat);
    virtual void ApplyTransforms(const Transform &transform, Camera *camera);

    DeclaredUniform::Id_t m_uniform_cube_matrices[6];

private:
    DeclaredUniform::Id_t m_uniform_camera_position,
        m_uniform_diffuse_color;
};
} // namespace hyperion

#endif
