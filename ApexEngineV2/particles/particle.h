#ifndef PARTICLE_H
#define PARTICLE_H

#include "../math/vector3.h"

namespace apex {
struct Particle {
    Vector3 m_position;
    Vector3 m_velocity;
    double m_life;
    double m_lifespan;
    bool m_alive;
};
} // namespace apex

#endif