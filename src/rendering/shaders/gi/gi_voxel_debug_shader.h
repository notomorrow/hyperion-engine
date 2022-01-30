#ifndef GI_VOXEL_DEBUG_SHADER_H
#define GI_VOXEL_DEBUG_SHADER_H

#include "../lighting_shader.h"

namespace hyperion {
class GIVoxelDebugShader : public LightingShader {
public:
    GIVoxelDebugShader(const ShaderProperties &properties);
    virtual ~GIVoxelDebugShader() = default;

    virtual void ApplyMaterial(const Material &mat);
    virtual void ApplyTransforms(const Transform &transform, Camera *camera);
};
} // namespace hyperion

#endif
