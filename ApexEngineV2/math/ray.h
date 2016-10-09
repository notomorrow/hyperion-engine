#ifndef RAY_H
#define RAY_H

#include "vector3.h"

namespace apex {

struct Ray {
    Vector3 m_position;
    Vector3 m_direction;
};

} // namespace apex

#endif