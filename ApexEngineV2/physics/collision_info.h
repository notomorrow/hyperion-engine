#ifndef APEX_PHYSICS_COLLISION_INFO_H
#define APEX_PHYSICS_COLLISION_INFO_H

#include "physics_material.h"
#include "../math/vector3.h"

#include <array>

namespace apex {
namespace physics {
// forward declaration
class Rigidbody;
struct CollisionInfo {
    Vector3 m_contact_point;
    Vector3 m_contact_normal;
    double m_contact_penetration;

    Matrix3 m_contact_to_world;
    Vector3 m_contact_velocity;
    double m_desired_delta_velocity;
    std::array<Vector3, 2> m_relative_contact_position;

    PhysicsMaterial m_combined_material;
    std::array<Rigidbody*, 2> m_bodies;

    CollisionInfo();
    CollisionInfo(const CollisionInfo &other);
};
} // namespace physics
} // namespace apex

#endif
