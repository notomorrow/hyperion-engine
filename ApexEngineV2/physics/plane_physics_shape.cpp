#include "plane_physics_shape.h"
#include "box_physics_shape.h"
#include "sphere_physics_shape.h"

namespace apex {
namespace physics {
PlanePhysicsShape::PlanePhysicsShape(const Vector3 &direction, double offset)
    : PhysicsShape(PhysicsShape_plane), 
      m_direction(direction), 
      m_offset(offset)
{
}

PlanePhysicsShape::PlanePhysicsShape(const PlanePhysicsShape &other)
    : PhysicsShape(PhysicsShape_plane),
      m_direction(other.m_direction),
      m_offset(other.m_offset)
{
}

bool PlanePhysicsShape::CollidesWith(BoxPhysicsShape *shape, CollisionList &out)
{
    // implementation is in BoxPhysicsShape
    return shape->CollidesWith(this, out);
}

bool PlanePhysicsShape::CollidesWith(SpherePhysicsShape *shape, CollisionList &out)
{
    // implementation is in SpherePhysicsShape
    return shape->CollidesWith(this, out);
}

bool PlanePhysicsShape::CollidesWith(PlanePhysicsShape *shape, CollisionList &out)
{
    return false;
}
} // namespace physics
} // namespace apex