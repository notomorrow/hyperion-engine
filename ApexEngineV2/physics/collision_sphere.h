#ifndef COLLISION_SPHERE_H
#define COLLISION_SPHERE_H

#include "collision_shape.h"

namespace apex {
class CollisionSphere : public CollisionShape {
public:
    double m_radius;
};
} // namespace apex

#endif