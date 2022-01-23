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
};
} // namespace hyperion

#endif
