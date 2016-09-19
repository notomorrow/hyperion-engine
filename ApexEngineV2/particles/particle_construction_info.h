#ifndef PARTICLE_CONSTRUCTION_INFO_H
#define PARTICLE_CONSTRUCTION_INFO_H

#include "particle.h"
#include "../rendering/material.h"
#include "../math/vector3.h"

#include <functional>

namespace apex {
struct ParticleConstructionInfo {
    // the function used to create a particle's origin
    std::function<Vector3(const Particle &particle)> m_origin_generator;
    // the function used to create a particle's velocity
    std::function<Vector3(const Particle &particle)> m_velocity_generator;
    // max number of particles allowed
    size_t m_max_particles;
    // the standard lifespan of a particle
    double m_lifespan;
    // the lifespan of a particle is m_lifespan +/- a random value in this range
    double m_lifespan_randomness;
    // gravity of each particle
    Vector3 m_gravity;
    // the material that will be applied to each particle
    Material m_material;

    ParticleConstructionInfo(std::function<Vector3(const Particle &particle)> m_origin_generator, 
        std::function<Vector3(const Particle &particle)> m_velocity_generator,
        size_t m_max_particles = 50, double m_lifespan = 2.0, double m_lifespan_randomness = 2.0);
    ParticleConstructionInfo(const ParticleConstructionInfo &other);
};

} // namespace apex

#endif