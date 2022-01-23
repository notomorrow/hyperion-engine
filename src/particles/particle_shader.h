#ifndef PARTICLE_SHADER_H
#define PARTICLE_SHADER_H

#include "../rendering/shader.h"

namespace hyperion {
class ParticleShader : public Shader {
public:
    ParticleShader(const ShaderProperties &properties);

    virtual void ApplyMaterial(const Material &mat);
    virtual void ApplyTransforms(const Transform &transform, Camera *camera);
};
} // namespace hyperion

#endif
