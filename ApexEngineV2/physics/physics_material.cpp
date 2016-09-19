#include "physics_material.h"

namespace apex {
namespace physics {
PhysicsMaterial::PhysicsMaterial(double mass, double friction, double restitution, 
    double linear_damping, double angular_damping)
    : m_mass(mass), 
      m_friction(friction), 
      m_restitution(restitution), 
      m_linear_damping(linear_damping), 
      m_angular_damping(angular_damping)
{
}

PhysicsMaterial::PhysicsMaterial(const PhysicsMaterial &other)
    : m_mass(other.m_mass), 
      m_friction(other.m_friction), 
      m_restitution(other.m_restitution),
      m_linear_damping(other.m_linear_damping),
      m_angular_damping(other.m_angular_damping)
{
}
} // namespace physics
} // namespace apex