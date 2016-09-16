#ifndef APEX_PHYSICS_COLLISION_H
#define APEX_PHYSICS_COLLISION_H

#include "collision_info.h"
#include "../../math/vector3.h"

#include <array>

#define COLLISION_VELOCITY_LIMIT 0.25
#define COLLISION_ANGULAR_LIMIT 0.2

namespace apex {
namespace physics {
// a class with static functions for working with collisions
class Collision {
public:
    // apply a velocity change to a collision
    static void ApplyVelocityChange(CollisionInfo &collision, 
        std::array<Vector3, 2> &linear_change, std::array<Vector3, 2> &angular_change);

    // apply a position change to a collision
    static void ApplyPositionChange(CollisionInfo &collision,
        std::array<Vector3, 2> &linear_change, std::array<Vector3, 2> &angular_change, double penetration);

    static void CalculateInternals(CollisionInfo &collision, double dt);

    static void SwapBodies(CollisionInfo &collision);

    static void MatchAwakeState(CollisionInfo &collision);

    static Vector3 CalculateLocalVelocity(CollisionInfo &collision,
        unsigned int body_index, double dt);

    static void CalculateContactBasis(CollisionInfo &collision);

    static void CalculateDesiredDeltaVelocity(CollisionInfo &collision, double dt);

    static Vector3 CalculateFrictionImpulse(CollisionInfo &collision,
        const std::array<Matrix3, 2> &inverse_inertia_tensor);

    static Vector3 CalculateFrictionlessImpulse(CollisionInfo &collision,
        const std::array<Matrix3, 2> &inverse_inertia_tensor);
};
} // namespace physics
} // namespace apex

#endif