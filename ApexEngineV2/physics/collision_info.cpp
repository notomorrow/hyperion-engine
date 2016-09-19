#include "collision_info.h"

namespace apex {
namespace physics {
CollisionInfo::CollisionInfo()
    : m_contact_penetration(0.0)
{
    m_bodies = { nullptr };
}

CollisionInfo::CollisionInfo(const CollisionInfo &other)
    : m_contact_point(other.m_contact_point),
      m_contact_normal(other.m_contact_normal),
      m_contact_penetration(other.m_contact_penetration),
      m_contact_to_world(other.m_contact_to_world),
      m_contact_velocity(other.m_contact_velocity),
      m_desired_delta_velocity(other.m_desired_delta_velocity),
      m_relative_contact_position(other.m_relative_contact_position),
      m_combined_material(other.m_combined_material),
      m_bodies(other.m_bodies)
{
}
} // namespace physics
} // namespace apex