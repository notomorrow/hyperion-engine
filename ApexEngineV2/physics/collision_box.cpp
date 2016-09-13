#include "collision_box.h"

namespace apex {
CollisionBox::CollisionBox(RigidBody *body, const Vector3 &dimensions)
    : CollisionShape(body), m_dimensions(dimensions)
{
}

CollisionBox::CollisionBox(const CollisionBox &other)
    : CollisionShape(other.m_body, other.m_transform, other.m_offset), m_dimensions(other.m_dimensions)
{
}

double CollisionBox::TransformToAxis(const Vector3 &axis) const
{
    return (m_dimensions.GetX() / 2.0) * fabs(axis.Dot(GetAxis(0))) +
        (m_dimensions.GetY() / 2.0) * fabs(axis.Dot(GetAxis(1))) +
        (m_dimensions.GetZ() / 2.0) * fabs(axis.Dot(GetAxis(2)));
}
} // namespace apex