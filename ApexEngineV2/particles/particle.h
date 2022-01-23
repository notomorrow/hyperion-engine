#ifndef PARTICLE_H
#define PARTICLE_H

#include "../math/vector3.h"

namespace hyperion {
struct Particle {
    Vector3 m_origin;
    Vector3 m_position;
    Vector3 m_global_position;
    Vector3 m_scale;
    Vector3 m_global_scale;
    Vector3 m_velocity;
    double m_camera_distance;
    double m_mass;
    double m_life;
    double m_lifespan;
    bool m_alive;

    bool operator<(const Particle &other) const;
};
} // namespace hyperion

#endif
