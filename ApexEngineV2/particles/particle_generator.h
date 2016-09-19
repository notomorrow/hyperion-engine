#ifndef PARTICLE_GENERATOR_H
#define PARTICLE_GENERATOR_H

#include "particle.h"
#include "particle_construction_info.h"
#include "particle_shader.h"
#include "../rendering/renderable.h"
#include "../rendering/camera/camera.h"

#include <vector>
#include <memory>

namespace apex {
class ParticleRenderer : public Renderable {
public:
    ParticleRenderer(const ParticleConstructionInfo &info);
    ~ParticleRenderer();

    void ResetParticle(Particle &particle);
    void UpdateParticles(Camera *camera, double dt);
    void DrawParticles(std::vector<Particle> &particles, 
        const ParticleConstructionInfo &info, Camera *camera);

    ParticleConstructionInfo m_info;

private:
    std::shared_ptr<ParticleShader> m_shader;
   // std::vector<Particle> m_particles;

    bool m_is_created;

    unsigned int m_vertex_buffer;
    unsigned int m_position_buffer;
    unsigned int m_lifespan_buffer;
};
} // namespace apex

#endif