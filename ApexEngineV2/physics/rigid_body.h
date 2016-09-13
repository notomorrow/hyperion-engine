#ifndef RIGID_BODY_H
#define RIGID_BODY_H

#include "../math/quaternion.h"
#include "../math/vector3.h"
#include "../math/matrix3.h"
#include "../math/matrix4.h"
#include "../math/transform.h"

#include <array>

namespace apex {
class RigidBody {
public:
    static const double SLEEP_EPSILON;

    RigidBody(double mass);

    inline double GetMass() const { return m_inverse_mass == 0.0 ? DBL_MAX : 1.0 / m_inverse_mass; }
    inline void SetMass(double mass) { m_inverse_mass = (mass == 0.0) ? 0.0 : 1.0 / mass; }
    inline double GetInverseMass() const { return m_inverse_mass; }
    inline void SetInverseMass(double inverse_mass) { m_inverse_mass = inverse_mass; }
    inline bool HasFiniteMass() const { return m_inverse_mass > 0.0; }

    inline const Vector3 &GetPosition() const { return m_position; }
    inline void SetPosition(const Vector3 &position) { m_position = position; }
    inline const Vector3 &GetVelocity() const { return m_velocity; }
    inline void SetVelocity(const Vector3 &velocity) { m_velocity = velocity; }
    inline const Vector3 &GetRotation() const { return m_rotation; }
    inline void SetRotation(const Vector3 &rotation) { m_rotation = rotation; }
    
    inline Matrix3 GetInertiaTensor() const { Matrix3 result(m_inverse_inertia_tensor); result.Invert(); return result; }
    inline void SetInertiaTensor(const Matrix3 &inertia_tensor) { m_inverse_inertia_tensor = inertia_tensor; m_inverse_inertia_tensor.Invert(); }
    inline const Matrix3 &GetInverseInertiaTensor() const { return m_inverse_inertia_tensor; }
    inline void SetInverseInertiaTensor(const Matrix3 &inverse_inertia_tensor) { m_inverse_inertia_tensor = inverse_inertia_tensor; }
    inline const Matrix3 &GetInverseInertiaTensorWorld() const { return m_inverse_inertia_tensor_world; }
    
    inline void SetLinearDamping(double linear_damping) { m_linear_damping = linear_damping; }
    inline double GetLinearDamping() const { return m_linear_damping; }
    inline void SetAngularDamping(double angular_damping) { m_angular_damping = angular_damping; }
    inline double GetAngularDamping() const { return m_angular_damping; }

    inline void ApplyForce(const Vector3 &force) { m_force_accum += force; m_is_awake = true; }
    
    inline bool IsAwake() const { return m_is_awake; }
    void SetAwake(bool awake);

    void ClearAccumulators();

    void CalculateDerivedData();
    void Integrate(double dt);

public://protected:
    double m_inverse_mass;
    double m_linear_damping;
    double m_angular_damping;
    Matrix3 m_inverse_inertia_tensor;
    Matrix3 m_inverse_inertia_tensor_world;
    double m_motion;
    bool m_is_awake;
    bool m_can_sleep;

    Vector3 m_position;
    Vector3 m_velocity;
    Vector3 m_rotation;
    Quaternion m_orientation;
    //Transform m_Transform;
    Matrix4 m_transform;
    Vector3 m_force_accum;
    Vector3 m_torque_accum;
    Vector3 m_acceleration;
    Vector3 m_last_acceleration;
};
}

#endif