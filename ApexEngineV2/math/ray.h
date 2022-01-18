#ifndef RAY_H
#define RAY_H

#include "vector3.h"

#include <vector>

namespace apex {

struct Ray {
    Vector3 m_position;
    Vector3 m_direction;
};

struct RaytestHit {
    Vector3 hitpoint;
    Vector3 normal;
};

using RaytestHitList_t = std::vector<RaytestHit>;

} // namespace apex

#endif
