#include "collision_sphere.h"

namespace apex {
CollisionSphere::CollisionSphere(RigidBody *body, double radius)
    : CollisionShape(body), m_radius(radius)
{
}

CollisionSphere::CollisionSphere(const CollisionSphere &other)
    : CollisionShape(other.m_body, other.m_transform, other.m_offset), m_radius(other.m_radius)
{
}
} // namespace apex