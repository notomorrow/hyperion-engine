#ifndef GI_VOXEL_SHADER_H
#define GI_VOXEL_SHADER_H

#include "../../shader.h"

namespace hyperion {
class GIVoxelShader : public Shader {
public:
    GIVoxelShader(const ShaderProperties &properties);
    virtual ~GIVoxelShader() = default;

    virtual void ApplyMaterial(const Material &mat);
    virtual void ApplyTransforms(const Transform &transform, Camera *camera);

private:
    DeclaredUniform::Id_t m_uniform_albedo,
        m_uniform_emissiveness,
        m_uniform_camera_position;
    // vct
    DeclaredUniform::Id_t m_uniform_voxel_map,
        m_uniform_voxel_probe_position,
        m_uniform_voxel_scene_scale;
};
} // namespace hyperion

#endif
