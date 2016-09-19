#include "particle_construction_info.h"
#include <cassert>

namespace apex {
ParticleConstructionInfo::ParticleConstructionInfo(std::function<Vector3(const Particle &particle)> origin_generator, 
    std::function<Vector3(const Particle &particle)> velocity_generator,
    size_t max_particles, double lifespan, double lifespan_randomness)
    : m_origin_generator(origin_generator),
      m_velocity_generator(velocity_generator),
      m_max_particles(max_particles),
      m_lifespan(lifespan),
      m_lifespan_randomness(lifespan_randomness)
{
    assert(m_lifespan > 0.0);
}

ParticleConstructionInfo::ParticleConstructionInfo(const ParticleConstructionInfo &other)
    : m_origin_generator(other.m_origin_generator),
      m_velocity_generator(other.m_velocity_generator),
      m_max_particles(other.m_max_particles),
      m_lifespan(other.m_lifespan),
      m_lifespan_randomness(other.m_lifespan_randomness),
      m_gravity(other.m_gravity),
      m_material(other.m_material)
{
    assert(m_lifespan > 0.0);
}
} // namespace apex