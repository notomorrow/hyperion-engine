#include "particle_emitter_control.h"
#include "../math/math_util.h"
#include "../entity.h"
#include <cassert>

namespace apex {
ParticleEmitterControl::ParticleEmitterControl(Camera *camera)
    : EntityControl(30.0),
      m_camera(camera)
{
}

void ParticleEmitterControl::ResetParticle(Particle &particle)
{
    double lifespan_random = MathUtil::EPSILON +
        MathUtil::Random(0.0, fabs(m_renderer->m_info.m_lifespan_randomness));

    particle.m_position = m_renderer->m_info.m_origin_generator(particle);
    particle.m_global_position = parent->GetGlobalTransform().GetTranslation() + particle.m_position;
    particle.m_velocity = m_renderer->m_info.m_velocity_generator(particle);
    particle.m_camera_distance = 0.0;
    particle.m_mass = 0.01 + MathUtil::Random(0.0f, 0.1f);
    particle.m_life = 0.0;
    particle.m_lifespan = m_renderer->m_info.m_lifespan + lifespan_random;
    particle.m_alive = true;
}

void ParticleEmitterControl::OnAdded()
{
    if (parent->GetRenderable() == nullptr ||
        (m_renderer = dynamic_cast<ParticleRenderer*>(parent->GetRenderable().get())) == nullptr) {
        throw "parent->GetRenderable() must be a pointer to ParticleRenderer!";
    }

    m_renderer->m_particles = &m_particles;

    // add all particles
    m_particles.resize(m_renderer->m_info.m_max_particles);
    for (Particle &particle : m_particles) {
        ResetParticle(particle);
    }
}

void ParticleEmitterControl::OnRemoved()
{
    m_renderer->m_particles = nullptr;
}

void ParticleEmitterControl::OnUpdate(double dt)
{
    assert(m_particles.size() <= m_renderer->m_info.m_max_particles);

    for (Particle &particle : m_particles) {
        if (particle.m_alive) {
            particle.m_life += dt;
            particle.m_velocity += m_renderer->m_info.m_gravity * particle.m_mass * dt;
            particle.m_position += particle.m_velocity * dt;
            particle.m_global_position = parent->GetGlobalTransform().GetTranslation() + particle.m_position;
            particle.m_camera_distance = particle.m_global_position.Distance(m_camera->GetTranslation());
        } else {
            // reset the particle if it is passed it's lifespan.
            ResetParticle(particle);
        }

        if (particle.m_life >= particle.m_lifespan) {
            // particle has passed it's lifespan, no longer alive
            particle.m_alive = false;
        }
    }

    // sort particles so that the closest particles are rendered first
    std::sort(m_particles.begin(), m_particles.end());
}
} // namespace apex
