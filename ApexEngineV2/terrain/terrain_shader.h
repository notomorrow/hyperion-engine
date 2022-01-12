#ifndef TERRAIN_SHADER_H
#define TERRAIN_SHADER_H

#include "../rendering/shader.h"

namespace apex {
class TerrainShader : public Shader {
public:
    TerrainShader(const ShaderProperties &properties);
    virtual ~TerrainShader() = default;

    virtual void ApplyMaterial(const Material &mat);
    virtual void ApplyTransforms(const Transform &transform, Camera *camera);
};
} // namespace apex

#endif
