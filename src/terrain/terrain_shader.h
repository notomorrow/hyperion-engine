#ifndef TERRAIN_SHADER_H
#define TERRAIN_SHADER_H

#include "../rendering/shaders/lighting_shader.h"

namespace hyperion {
class TerrainShader : public LightingShader {
public:
    TerrainShader(const ShaderProperties &properties);
    virtual ~TerrainShader() = default;

    virtual void ApplyMaterial(const Material &mat) override;

protected:
    DeclaredUniform::Id_t m_uniform_base_terrain_scale;
};
} // namespace hyperion

#endif
