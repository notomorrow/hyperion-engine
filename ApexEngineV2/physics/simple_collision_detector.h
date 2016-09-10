#ifndef SIMPLE_COLLISION_DETECTOR_H
#define SIMPLE_COLLISION_DETECTOR_H

#include "collision_sphere.h"
#include "collision_plane.h"
#include "collision_box.h"

namespace apex {
class SimpleCollisionDetector {
public:
    static bool SphereAndHalfSpace(const CollisionSphere &sphere, const CollisionPlane &plane);
    static bool SphereAndSphere(const CollisionSphere &a, const CollisionSphere &b);
    static bool BoxAndHalfSpace(const CollisionBox &box, const CollisionPlane &plane);
    static bool BoxAndBox(const CollisionBox &a, const CollisionBox &b);
};
} // namespace apex

#endif