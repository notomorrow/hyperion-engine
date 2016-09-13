#ifndef COLLISION_SPHERE_H
#define COLLISION_SPHERE_H

#include "collision_shape.h"

namespace apex {
class CollisionSphere : public CollisionShape {
public:
    CollisionSphere(RigidBody *body, double radius);
    CollisionSphere(const CollisionSphere &other);

    inline double GetRadius() const { return m_radius; }
    inline void SetRadius(double radius) { m_radius = radius; }

protected:
    double m_radius;
};
} // namespace apex

#endif