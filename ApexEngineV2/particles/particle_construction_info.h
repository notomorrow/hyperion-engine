#ifndef PARTICLE_CONSTRUCTION_INFO_H
#define PARTICLE_CONSTRUCTION_INFO_H

#include "particle.h"
#include "../rendering/material.h"
#include "../math/vector3.h"

#include <functional>

namespace apex {
struct ParticleConstructionInfo {
    /*// the function used to create a particle's origin
    std::function<Vector3(const Particle &particle)> m_origin_generator;
    // the function used to create a particle's velocity
    std::function<Vector3(const Particle &particle)> m_velocity_generator;*/

    Vector3 m_origin;

    Vector3 m_origin_randomness;

    Vector3 m_velocity;

    Vector3 m_velocity_randomness;

    double m_mass;

    double m_mass_randomness;

    // max number of particles allowed
    size_t m_max_particles;
    // the standard lifespan of a particle
    double m_lifespan;
    // the lifespan of a particle is m_lifespan +/- a random value in this range
    double m_lifespan_randomness;
    // gravity of each particle
    Vector3 m_gravity;
    // scale of each particle
    Vector3 m_scale;
    // scale randomness
    Vector3 m_scale_randomness;
    // the material that will be applied to each particle
    Material m_material;

    ParticleConstructionInfo(const Vector3 &origin = Vector3::Zero(), const Vector3 &origin_randomness = Vector3(0.5),
        const Vector3 &velocity = Vector3::Zero(), const Vector3 &velocity_randomness = Vector3(0.5),
        double mass = 0.1, double mass_randomness = 0.1,
        size_t max_particles = 50,
        double lifespan = 2.0, double lifespan_randomness = 2.0,
        const Vector3 &scale = Vector3::One(), const Vector3 &scale_randomness = Vector3::Zero());
    ParticleConstructionInfo(const ParticleConstructionInfo &other);
};

} // namespace apex

#endif
