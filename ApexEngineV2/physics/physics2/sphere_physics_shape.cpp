#include "sphere_physics_shape.h"
#include "box_physics_shape.h"
#include "plane_physics_shape.h"

namespace apex {
namespace physics {
SpherePhysicsShape::SpherePhysicsShape(double radius)
    : PhysicsShape(PhysicsShape_sphere), m_radius(radius)
{
}

bool SpherePhysicsShape::CollidesWith(BoxPhysicsShape *shape, CollisionList &out)
{
    // implementation is in BoxPhysicsShape
    bool collides = shape->CollidesWith(this, out);
    for (CollisionInfo &info : out.m_collisions) {
        info.m_contact_normal *= -1.0f; // reverse the normal
    }
    return collides;
}

bool SpherePhysicsShape::CollidesWith(SpherePhysicsShape *shape, CollisionList &out)
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

    CollisionInfo collision;
    collision.m_contact_point = contact_point;
    collision.m_contact_normal = contact_normal;
    collision.m_contact_penetration = penetration;
    out.m_collisions.push_back(collision);

    return true;
}

bool SpherePhysicsShape::CollidesWith(PlanePhysicsShape *shape, CollisionList &out)
{
    Vector3 position = GetAxis(3);
    double dist = shape->GetDirection().Dot(position) - m_radius - shape->GetOffset();
    if (dist >= 0) {
        return false;
    }

    Vector3 contact_point = position - shape->GetDirection() *
        (dist - m_radius);

    CollisionInfo collision;
    collision.m_contact_normal = shape->GetDirection();
    collision.m_contact_penetration = -dist;
    collision.m_contact_point = contact_point;
    out.m_collisions.push_back(collision);

    return true;
}
} // namespace physics
} // namespace apex