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

    bool CollidesWith(BoxPhysicsShape *shape, CollisionInfo &out);
    bool CollidesWith(SpherePhysicsShape *shape, CollisionInfo &out);
    bool CollidesWith(PlanePhysicsShape *shape, CollisionInfo &out);

private:
    double m_radius;
};
} // namespace physics
} // namespace apex

#endif