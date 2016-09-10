#ifndef RIGID_BODY_H
#define RIGID_BODY_H

#include "../math/quaternion.h"
#include "../math/vector3.h"
#include "../math/matrix3.h"
#include "../math/matrix4.h"

#include <array>

namespace apex {
class RigidBody {
public:
    RigidBody(double mass);

    double GetMass() const;
    void SetMass(double mass);
    double GetInverseMass() const;
    void SetInverseMass(double inv_mass);
    bool HasFiniteMass() const;

    const Matrix3 &GetInertiaTensor() const;
    void SetInertiaTensor(const Matrix3 &mat);
    const Matrix3 &GetInverseInertiaTensor() const;
    void SetInverseInertiaTensor(const Matrix3 &mat);
    const Matrix3 &GetInverseInertiaTensorWorld() const;

    void SetLinearDamping(double d);
    double GetLinearDamping() const;
    void SetAngularDamping(double d);
    double GetAngularDamping() const;

    void ApplyForce(const Vector3 &force);

    bool IsAwake() const;
    void SetAwake(bool b);

    void ClearAccumulators();

    void CalculateDerivedData();
    void Integrate(double dt);

public://protected:
    double inverse_mass;
    double linear_damping;
    double angular_damping;
    Matrix3 inverse_inertia_tensor;
    Matrix3 inverse_inertia_tensor_world;
    double motion;
    bool is_awake;
    bool can_sleep;

    Vector3 position;
    Vector3 velocity;
    Vector3 rotation;
    Quaternion orientation;
    Matrix4 transform;
    Vector3 force_accum;
    Vector3 torque_accum;
    Vector3 acceleration;
    Vector3 last_acceleration;
};
}

#endif