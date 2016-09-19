#include "particle.h"

namespace apex {
bool Particle::operator<(const Particle &other) const
{
    return m_camera_distance > other.m_camera_distance;
}
} // namespace apex