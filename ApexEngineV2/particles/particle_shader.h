#ifndef PARTICLE_SHADER_H
#define PARTICLE_SHADER_H

#include "../rendering/shader.h"

namespace apex {
class ParticleShader : public Shader {
public:
    ParticleShader(const ShaderProperties &properties);

    virtual void ApplyMaterial(const Material &mat);
    virtual void ApplyTransforms(const Matrix4 &transform, Camera *camera);
};
} // namespace apex

#endif