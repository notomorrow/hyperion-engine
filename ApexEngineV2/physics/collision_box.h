#ifndef COLLISION_BOX_H
#define COLLISION_BOX_H

#include "collision_shape.h"
#include "../math/vector3.h"

namespace apex {
class CollisionBox : public CollisionShape {
public:
    Vector3 m_half_size;

    double TransformToAxis(const Vector3 &axis) const;
};
} // namespace apex

#endif