#ifndef APEX_PHYSICS_BOX_COLLISION_H
#define APEX_PHYSICS_BOX_COLLISION_H

#include "collision_info.h"
#include "box_physics_shape.h"
#include "../../math/vector3.h"

namespace apex {
namespace physics {
class BoxCollision {
public:
    static double TransformToAxis(const BoxPhysicsShape &box, const Vector3 &axis);
    static void FillPointFaceBoxBox(const BoxPhysicsShape &a, const BoxPhysicsShape &b,
        const Vector3 &to_center, CollisionInfo &out,
        unsigned int best, double penetration);
    static Vector3 ContactPoint(const Vector3 &a_point, const Vector3 &a_dir, double a_size,
        const Vector3 &b_point, const Vector3 &b_dir, double b_size,
        bool outside_edge);
    static double PenetrationOnAxis(const BoxPhysicsShape &a, const BoxPhysicsShape &b,
        const Vector3 &axis, const Vector3 &to_center);
    static bool TryAxis(const BoxPhysicsShape &a, const BoxPhysicsShape &b,
        Vector3 axis, const Vector3 &to_center, unsigned int index,
        double &out_smallest_penetration, unsigned int &out_smallest_case);
};
} // namespace physics
} // namespace apex

#endif