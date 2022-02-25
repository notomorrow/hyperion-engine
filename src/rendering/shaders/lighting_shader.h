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

private:
    DeclaredUniform::Id_t m_uniform_shininess,
        m_uniform_roughness,
        m_uniform_parallax_height,
        m_uniform_camera_position,
        m_uniform_env_global_cubemap,
        m_uniform_shadow_split[4],
        m_uniform_shadow_matrix[4],
        m_uniform_shadow_map[4],
        m_uniform_diffuse_color,
        m_uniform_poisson_disk[16];
    // lights
    DeclaredUniform::Id_t m_uniform_directional_light_direction,
        m_uniform_directional_light_color,
        m_uniform_directional_light_intensity;
    // envmap
    DeclaredUniform::Id_t m_uniform_envmap_origin,
        m_uniform_envmap_max,
        m_uniform_envmap_min;
    // vct
    DeclaredUniform::Id_t m_uniform_voxel_map,
        m_uniform_voxel_probe_position,
        m_uniform_voxel_scene_scale;
    // sh
    DeclaredUniform::Id_t m_uniform_sh_map,
        m_uniform_has_sh_map;
};
} // namespace hyperion

#endif
