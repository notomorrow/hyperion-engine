#include "plane_physics_shape.h"
#include "box_physics_shape.h"
#include "sphere_physics_shape.h"

namespace apex {
namespace physics {
PlanePhysicsShape::PlanePhysicsShape(const Vector3 &direction, double offset)
    : PhysicsShape(PhysicsShape_plane), m_direction(direction), m_offset(offset)
{
}

bool PlanePhysicsShape::CollidesWith(BoxPhysicsShape *shape, CollisionList &out)
{
    // implementation is in BoxPhysicsShape
    bool collides = shape->CollidesWith(this, out);
    return collides;
}

bool PlanePhysicsShape::CollidesWith(SpherePhysicsShape *shape, CollisionList &out)
{
    // implementation is in SpherePhysicsShape
    bool collides = shape->CollidesWith(this, out);
    return collides;
}

bool PlanePhysicsShape::CollidesWith(PlanePhysicsShape *shape, CollisionList &out)
{
    return false;
}
} // namespace physics
} // namespace apex