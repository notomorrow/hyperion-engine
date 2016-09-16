#ifndef APEX_PHYSICS_SPHERE_PHYSICS_SHAPE_H
#define APEX_PHYSICS_SPHERE_PHYSICS_SHAPE_H

#include "physics_shape.h"

namespace apex {
namespace physics {
class SpherePhysicsShape : public PhysicsShape {
public:
    SpherePhysicsShape(double radius);

    inline double GetRadius() const { return m_radius; }
    inline void SetRadius(double radius) { m_radius = radius; }

    bool CollidesWith(BoxPhysicsShape *shape, CollisionList &out);
    bool CollidesWith(SpherePhysicsShape *shape, CollisionList &out);
    bool CollidesWith(PlanePhysicsShape *shape, CollisionList &out);

private:
    double m_radius;
};
} // namespace physics
} // namespace apex

#endif