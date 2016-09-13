#ifndef CONTACT_H
#define CONTACT_H

#include "rigid_body.h"

#include <array>

#define CONTACT_VELOCITY_LIMIT 0.25
#define CONTACT_ANGULAR_LIMIT 0.2

#define MAX_CONTACTS 25

namespace apex {
struct PotentialContact {
    std::array<RigidBody*, 2> m_bodies;
};

class Contact {
    friend class ContactResolver;
public:
    Contact();
    Contact(RigidBody *a, RigidBody *b);

    inline RigidBody *GetBodyA() const { return m_bodies[0]; }
    inline void SetBodyA(RigidBody *body) { m_bodies[0] = body; }
    inline RigidBody *GetBodyB() const { return m_bodies[1]; }
    inline void SetBodyB(RigidBody *body) { m_bodies[1] = body; }
    inline double GetFriction() const { return m_friction; }
    inline void SetFriction(double friction) { m_friction = friction; }
    inline double GetRestitution() const { return m_restitution; }
    inline void SetRestitution(double restitution) { m_restitution = restitution; }
    inline const Vector3 &GetContactPoint() const { return m_contact_point; }
    inline void SetContactPoint(const Vector3 &contact_point) { m_contact_point = contact_point; }
    inline const Vector3 &GetContactNormal() const { return m_contact_normal; }
    inline void SetContactNormal(const Vector3 &contact_normal) { m_contact_normal = contact_normal; }
    inline double GetContactPenetration() const { return m_contact_penetration; }
    inline void SetContactPenetration(double contact_penetration) { m_contact_penetration = contact_penetration; }

    void SetBodyData(RigidBody *one, RigidBody *two,
        double friction, double restitution);

protected:
    std::array<RigidBody*, 2> m_bodies;

    double m_friction;
    double m_restitution;
    Vector3 m_contact_point;
    Vector3 m_contact_normal;
    double m_contact_penetration;

    Matrix3 m_contact_to_world;
    Vector3 m_contact_velocity;
    double m_desired_delta_velocity;
    std::array<Vector3, 2> m_relative_contact_position;

    void CalculateInternals(double dt);
    void SwapBodies();
    void MatchAwakeState();
    void CalculateDesiredDeltaVelocity(double dt);
    Vector3 CalculateLocalVelocity(unsigned int body_index, double dt);
    void CalculateContactBasis();

    void ApplyImpulse(const Vector3 &impulse, RigidBody *body,
        Vector3 &out_velocity_change, Vector3 &out_rotation_change);
    void ApplyVelocityChange(std::array<Vector3, 2> &velocity_change, std::array<Vector3, 2> &rotation_change);
    void ApplyPositionChange(std::array<Vector3, 2> &linear_change, std::array<Vector3, 2> &angular_change, double penetration);

    Vector3 CalculateFrictionlessImpulse(const std::array<Matrix3, 2> &inverse_inertia_tensor);
    Vector3 CalculateFrictionImpulse(const std::array<Matrix3, 2> &inverse_inertia_tensor);
};
} // namespace apex

#endif