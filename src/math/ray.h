#ifndef HYPERION_RAY_H
#define HYPERION_RAY_H

#include "vector3.h"

#include <hash_code.h>
#include <types.h>

#include <core/lib/flat_set.h>

#include <array>
#include <tuple>

namespace hyperion {

class BoundingBox;
class RayTestResults;
struct RayHit;

struct Ray {
    Vector3 position;
    Vector3 direction;

    bool TestAabb(const BoundingBox &aabb, RayTestResults &out_results) const;
    bool TestAabb(const BoundingBox &aabb, int hit_id, RayTestResults &out_results) const;
    bool TestAabb(const BoundingBox &aabb, int hit_id, const void *user_data, RayTestResults &out_results) const;

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(position.GetHashCode());
        hc.Add(direction.GetHashCode());

        return hc;
    }
};

struct RayHit {
    static constexpr bool no_hit = false;
    
    Vector3     hitpoint;
    Vector3     normal;
    float       distance  = 0.0f;
    int         id        = ~0;
    const void *user_data = nullptr;

    bool operator<(const RayHit &other) const
    {
        return std::tie(
            distance,
            hitpoint,
            normal,
            id,
            user_data
        ) < std::tie(
            other.distance,
            other.hitpoint,
            other.normal,
            other.id,
            other.user_data
        );
    }

    bool operator==(const RayHit &other) const
    {
        return distance  == other.distance
            && hitpoint  == other.hitpoint
            && normal    == other.normal
            && id        == other.id
            && user_data == other.user_data;
    }

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(distance);
        hc.Add(hitpoint.GetHashCode());
        hc.Add(normal.GetHashCode());
        hc.Add(id);
        hc.Add(user_data);

        return hc;
    }
};

class RayTestResults : public FlatSet<RayHit> {
public:
    bool cumulative = true;
    
    bool AddHit(const RayHit &hit);
};

} // namespace hyperion

#endif
