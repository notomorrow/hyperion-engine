#include "sphere_physics_shape.h"
#include "box_physics_shape.h"

namespace apex {
namespace physics {
SpherePhysicsShape::SpherePhysicsShape(double radius)
    : PhysicsShape(PhysicsShape_sphere), m_radius(radius)
{
}

bool SpherePhysicsShape::CollidesWith(BoxPhysicsShape *shape, CollisionInfo &out)
{
    // implementation is in BoxPhysicsShape
    bool collides = shape->CollidesWith(this, out);
    out.m_contact_normal *= -1.0f; // reverse direction of normal
    return collides;
}

bool SpherePhysicsShape::CollidesWith(SpherePhysicsShape *shape, CollisionInfo &out)
{
    Vector3 a_position = GetAxis(3);
    Vector3 b_position = shape->GetAxis(3);

    Vector3 mid = a_position - b_position;
    double mag = double(mid.Length());

    if (mag <= 0.0 || mag >= (m_radius + shape->GetRadius())) {
        return false;
    }

    Vector3 contact_normal = mid * (1.0 / mag);
    Vector3 contact_point = a_position + (mid / 2.0);
    double penetration = (m_radius + shape->GetRadius()) - mag;

    out.m_contact_point = contact_point;
    out.m_contact_normal = contact_normal;
    out.m_contact_penetration = penetration;
    return true;
}
} // namespace physics
} // namespace apex