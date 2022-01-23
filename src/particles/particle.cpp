#include "particle.h"

namespace hyperion {
bool Particle::operator<(const Particle &other) const
{
    return m_camera_distance > other.m_camera_distance;
}
} // namespace hyperion
