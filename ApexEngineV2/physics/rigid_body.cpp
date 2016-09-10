#include "rigid_body.h"
#include "../math/math_util.h"
#include <cassert>

namespace apex {
static void CalculateTransformMatrix(Matrix4 &transform, const Vector3 &position,
    const Quaternion &orientation)
{
    transform.values[0] = 1 - 2 * orientation.GetY()*orientation.GetY() -
        2 * orientation.GetZ()*orientation.GetZ();
    transform.values[1] = 2 * orientation.GetX()*orientation.GetY() -
        2 * orientation.GetW()*orientation.GetZ();
    transform.values[2] = 2 * orientation.GetX()*orientation.GetZ() +
        2 * orientation.GetW()*orientation.GetY();
    transform.values[3] = position.x;

    transform.values[4] = 2 * orientation.GetX()*orientation.GetY() +
        2 * orientation.GetW()*orientation.GetZ();
    transform.values[5] = 1 - 2 * orientation.GetX()*orientation.GetX() -
        2 * orientation.GetZ()*orientation.GetZ();
    transform.values[6] = 2 * orientation.GetY()*orientation.GetZ() -
        2 * orientation.GetW()*orientation.GetX();
    transform.values[7] = position.y;

    transform.values[8] = 2 * orientation.GetX()*orientation.GetZ() -
        2 * orientation.GetW()*orientation.GetY();
    transform.values[9] = 2 * orientation.GetY()*orientation.GetZ() +
        2 * orientation.GetW()*orientation.GetX();
    transform.values[10] = 1 - 2 * orientation.GetX()*orientation.GetX() -
        2 * orientation.GetY()*orientation.GetY();
    transform.values[11] = position.z;
}

static void TransformInertiaTensor(Matrix3 &iit_world, const Quaternion &q,
    const Matrix3 &iit_body, const Matrix4 &rotmat)
{
    auto t4 = rotmat.values[0] * iit_body.values[0] +
        rotmat.values[1] * iit_body.values[3] +
        rotmat.values[2] * iit_body.values[6];
    auto t9 = rotmat.values[0] * iit_body.values[1] +
        rotmat.values[1] * iit_body.values[4] +
        rotmat.values[2] * iit_body.values[7];
    auto t14 = rotmat.values[0] * iit_body.values[2] +
        rotmat.values[1] * iit_body.values[5] +
        rotmat.values[2] * iit_body.values[8];
    auto t28 = rotmat.values[4] * iit_body.values[0] +
        rotmat.values[5] * iit_body.values[3] +
        rotmat.values[6] * iit_body.values[6];
    auto t33 = rotmat.values[4] * iit_body.values[1] +
        rotmat.values[5] * iit_body.values[4] +
        rotmat.values[6] * iit_body.values[7];
    auto t38 = rotmat.values[4] * iit_body.values[2] +
        rotmat.values[5] * iit_body.values[5] +
        rotmat.values[6] * iit_body.values[8];
    auto t52 = rotmat.values[8] * iit_body.values[0] +
        rotmat.values[9] * iit_body.values[3] +
        rotmat.values[10] * iit_body.values[6];
    auto t57 = rotmat.values[8] * iit_body.values[1] +
        rotmat.values[9] * iit_body.values[4] +
        rotmat.values[10] * iit_body.values[7];
    auto t62 = rotmat.values[8] * iit_body.values[2] +
        rotmat.values[9] * iit_body.values[5] +
        rotmat.values[10] * iit_body.values[8];

    iit_world.values[0] = t4 * rotmat.values[0] +
        t9 * rotmat.values[1] +
        t14 * rotmat.values[2];
    iit_world.values[1] = t4 * rotmat.values[4] +
        t9 * rotmat.values[5] +
        t14 * rotmat.values[6];
    iit_world.values[2] = t4 * rotmat.values[8] +
        t9 * rotmat.values[9] +
        t14 * rotmat.values[10];
    iit_world.values[3] = t28*rotmat.values[0] +
        t33 * rotmat.values[1] +
        t38 * rotmat.values[2];
    iit_world.values[4] = t28 * rotmat.values[4] +
        t33 * rotmat.values[5] +
        t38 * rotmat.values[6];
    iit_world.values[5] = t28 * rotmat.values[8] +
        t33 * rotmat.values[9] +
        t38 * rotmat.values[10];
    iit_world.values[6] = t52*rotmat.values[0] +
        t57 * rotmat.values[1] +
        t62 * rotmat.values[2];
    iit_world.values[7] = t52 * rotmat.values[4] +
        t57 * rotmat.values[5] +
        t62 * rotmat.values[6];
    iit_world.values[8] = t52 * rotmat.values[8] +
        t57 * rotmat.values[9] +
        t62 * rotmat.values[10];
}

RigidBody::RigidBody(double mass)
    : is_awake(true)
{
    SetMass(mass);
}

double RigidBody::GetMass() const
{
    return inverse_mass == 0.0 ? DBL_MAX : 1.0 / inverse_mass;
}

void RigidBody::SetMass(double mass)
{
    if (mass == 0.0) {
        inverse_mass = 0.0;
    } else {
        inverse_mass = 1.0 / mass;
    }
}

double RigidBody::GetInverseMass() const
{
    return inverse_mass;
}

void RigidBody::SetInverseMass(double inv_mass)
{
    inverse_mass = inv_mass;
}

bool RigidBody::HasFiniteMass() const
{
    return inverse_mass > 0.0;
}

const Matrix3 &RigidBody::GetInertiaTensor() const
{
    Matrix3 result(inverse_inertia_tensor);
    result.Invert();
    return result;
}

void RigidBody::SetInertiaTensor(const Matrix3 &mat)
{
    inverse_inertia_tensor = mat;
    inverse_inertia_tensor.Invert();
}

const Matrix3 &RigidBody::GetInverseInertiaTensor() const
{
    return inverse_inertia_tensor;
}

void RigidBody::SetInverseInertiaTensor(const Matrix3 &mat)
{
    inverse_inertia_tensor = mat;
}

const Matrix3 &RigidBody::GetInverseInertiaTensorWorld() const
{
    return inverse_inertia_tensor_world;
}

void RigidBody::SetLinearDamping(double d)
{
    linear_damping = d;
}

double RigidBody::GetLinearDamping() const
{
    return linear_damping;
}

void RigidBody::SetAngularDamping(double d)
{
    angular_damping = d;
}

double RigidBody::GetAngularDamping() const
{
    return angular_damping;
}

void RigidBody::ApplyForce(const Vector3 &force)
{
    force_accum += force;
    is_awake = true;
}

bool RigidBody::IsAwake() const
{
    return is_awake;
}

void RigidBody::SetAwake(bool b)
{
    if (b) {
        is_awake = true;
        motion = MathUtil::epsilon * 2.0;
    } else {
        is_awake = false;
        velocity = Vector3::Zero();
        rotation = Vector3::Zero();
    }
}

void RigidBody::ClearAccumulators()
{
    force_accum = Vector3::Zero();
    torque_accum = Vector3::Zero();
}

void RigidBody::CalculateDerivedData()
{
    orientation.Normalize();

    CalculateTransformMatrix(transform, position, orientation);
    TransformInertiaTensor(inverse_inertia_tensor_world, orientation,
        inverse_inertia_tensor, transform);
}

void RigidBody::Integrate(double dt)
{
    if (!is_awake) {
        return;
    }

    last_acceleration = acceleration;
    last_acceleration += force_accum * inverse_mass;

    Vector3 angular_acceleration = torque_accum * inverse_inertia_tensor_world;

    velocity += last_acceleration * dt;
    rotation += angular_acceleration * dt;

    velocity *= pow(linear_damping, dt);
    rotation *= pow(angular_damping, dt);

    position += velocity * dt;
    orientation += rotation * dt;

    CalculateDerivedData();
    ClearAccumulators();

    if (can_sleep) {
        double current_motion = velocity.Dot(velocity) + rotation.Dot(rotation);
        double bias = pow(0.5, dt);
        motion = bias * motion + (1.0 - bias) * current_motion;

        if (motion < MathUtil::epsilon) {
            SetAwake(false);
        } else if (motion > 10.0 * MathUtil::epsilon) {
            motion = 10.0 * MathUtil::epsilon;
        }
    }
}
} // namespace apex