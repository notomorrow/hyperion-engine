#ifndef COMPLEX_COLLISION_DETECTOR_H
#define COMPLEX_COLLISION_DETECTOR_H

#include "collision_data.h"
#include "collision_sphere.h"
#include "collision_plane.h"
#include "collision_box.h"

namespace apex {
class ComplexCollisionDetector {
public:
    static unsigned int SphereAndHalfSpace(const CollisionSphere &sphere, const CollisionPlane &plane,
        CollisionData &data);
    static unsigned int SphereAndTruePlane(const CollisionSphere &sphere, const CollisionPlane &plane,
        CollisionData &data);
    static unsigned int SphereAndSphere(const CollisionSphere &a, const CollisionSphere &b,
        CollisionData &data);
    static unsigned int BoxAndHalfSpace(const CollisionBox &box, const CollisionPlane &plane,
        CollisionData &data);
    static unsigned int BoxAndBox(const CollisionBox &a, const CollisionBox &b,
        CollisionData &data);
    static unsigned int BoxAndPoint(const CollisionBox &box, const Vector3 &point,
        CollisionData &data);
    static unsigned int BoxAndSphere(const CollisionBox &box, const CollisionSphere &sphere,
        CollisionData &data);
};
} // namespace apex

#endif