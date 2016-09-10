#ifndef COLLISION_PLANE_H
#define COLLISION_PLANE_H

#include "../math/vector3.h"

namespace apex {
class CollisionPlane {
public:
    Vector3 m_direction;
    double m_offset;
};
} // namespace apex

#endif