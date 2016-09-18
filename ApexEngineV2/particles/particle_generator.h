#ifndef PARTICLE_GENERATOR_H
#define PARTICLE_GENERATOR_H

#include "particle.h"
#include "particle_construction_info.h"
#include "particle_shader.h"
#include "../rendering/camera/camera.h"

#include <vector>
#include <memory>

namespace apex {
class ParticleGenerator {
public:
    ParticleGenerator(const ParticleConstructionInfo &info);
    ~ParticleGenerator();

    void ResetParticle(Particle &particle);
    Particle *FindUnusedParticle();
    void UpdateParticles(double dt);
    void DrawParticles(Camera *camera);

private:
    ParticleConstructionInfo m_info;

    std::shared_ptr<ParticleShader> m_shader;

    std::vector<Particle> m_particles;

    bool m_is_created;
    bool m_is_uploaded;

    unsigned int m_vertex_buffer;
    unsigned int m_position_buffer;
    unsigned int m_lifespan_buffer;
    unsigned int m_lookat_buffer;
};
} // namespace apex

#endif