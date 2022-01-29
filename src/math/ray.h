#ifndef RAY_H
#define RAY_H

#include "vector3.h"
#include "../hash_code.h"

#include <vector>

namespace hyperion {

struct Ray {
    Vector3 m_position;
    Vector3 m_direction;

    inline HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(m_position.GetHashCode());
        hc.Add(m_direction.GetHashCode());

        return hc;
    }
};

struct RaytestHit {
    Vector3 hitpoint;
    Vector3 normal;

    inline HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(hitpoint.GetHashCode());
        hc.Add(normal.GetHashCode());

        return hc;
    }
};

using RaytestHitList_t = std::vector<RaytestHit>;

} // namespace hyperion

#endif
