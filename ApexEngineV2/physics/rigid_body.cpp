#include "rigid_body.h"
#include "../math/math_util.h"
#include <cassert>

namespace apex {
const double RigidBody::SLEEP_EPSILON = 0.3;

static void CalculateTransformMatrix(Matrix4 &transform, const Vector3 &position,
    Quaternion orientation)
{
    orientation.Invert();

    Matrix4 R, T;

    MatrixUtil::ToRotation(R, orientation);
    MatrixUtil::ToTranslation(T, position);

    transform = R * T;
}

static void TransformInertiaTensor(Matrix3 &iit_world, const Quaternion &q,
    const Matrix3 &iit_body, Matrix4 rotmat)
{
    //rotmat.Transpose();
    auto t4 = rotmat(0, 0) * iit_body.values[0] +
        rotmat(0, 1) * iit_body.values[3] +
        rotmat(0, 2) * iit_body.values[6];
    auto t9 = rotmat(0, 0) * iit_body.values[1] +
        rotmat(0, 1) * iit_body.values[4] +
        rotmat(0, 2) * iit_body.values[7];
    auto t14 = rotmat(0, 0) * iit_body.values[2] +
        rotmat(0, 1)* iit_body.values[5] +
        rotmat(0, 2) * iit_body.values[8];
    auto t28 = rotmat(1, 0) * iit_body.values[0] +
        rotmat(1, 1) * iit_body.values[3] +
        rotmat(1, 2) * iit_body.values[6];
    auto t33 = rotmat(1, 0) * iit_body.values[1] +
        rotmat(1, 1) * iit_body.values[4] +
        rotmat(1, 2) * iit_body.values[7];
    auto t38 = rotmat(1, 0) * iit_body.values[2] +
        rotmat(1, 1) * iit_body.values[5] +
        rotmat(1, 2) * iit_body.values[8];
    auto t52 = rotmat(2, 0) * iit_body.values[0] +
        rotmat(2, 1) * iit_body.values[3] +
        rotmat(2, 2) * iit_body.values[6];
    auto t57 = rotmat(2, 0) * iit_body.values[1] +
        rotmat(2, 1) * iit_body.values[4] +
        rotmat(2, 2) * iit_body.values[7];
    auto t62 = rotmat(2, 0) * iit_body.values[2] +
        rotmat(2, 1) * iit_body.values[5] +
        rotmat(2, 2) * iit_body.values[8];

    iit_world.values[0] = t4 * rotmat(0, 0) +
        t9 * rotmat(0, 1) +
        t14 * rotmat(0, 2);
    iit_world.values[1] = t4 * rotmat(1, 0) +
        t9 * rotmat(1, 1) +
        t14 * rotmat(1, 2);
    iit_world.values[2] = t4 * rotmat(2, 0) +
        t9 * rotmat(2, 1) +
        t14 * rotmat(2, 2);
    iit_world.values[3] = t28*rotmat(0, 0) +
        t33 * rotmat(0, 1) +
        t38 * rotmat(0, 2);
    iit_world.values[4] = t28 * rotmat(1, 0) +
        t33 * rotmat(1, 1) +
        t38 * rotmat(1, 2);
    iit_world.values[5] = t28 * rotmat(2, 0) +
        t33 * rotmat(2, 1) +
        t38 * rotmat(2, 2);
    iit_world.values[6] = t52*rotmat(0, 0) +
        t57 * rotmat(0, 1) +
        t62 * rotmat(0, 2);
    iit_world.values[7] = t52 * rotmat(1, 0) +
        t57 * rotmat(1, 1) +
        t62 * rotmat(1, 2);
    iit_world.values[8] = t52 * rotmat(2, 0) +
        t57 * rotmat(2, 1) +
        t62 * rotmat(2, 2);
}

RigidBody::RigidBody(double mass)
    : m_is_awake(true), m_motion(0.0)
{
    assert(mass >= 1.0); // things seem to behave strangely if mass is < 1.0
    SetMass(mass);
}

void RigidBody::SetAwake(bool awake)
{
    m_is_awake = awake;
    if (m_is_awake) {
        m_motion = SLEEP_EPSILON * 2.0;
    } else {
        m_velocity = Vector3::Zero();
        m_rotation = Vector3::Zero();
    }

    Vector3 testvec(1, 2, 4);
    Quaternion testrot(Vector3(7, 7, 1), 0.3);
    Matrix4 transform1;
    CalculateTransformMatrix(transform1, testvec, testrot);
    std::cout << "Transform1 = " << transform1 << "\n\n";

    Matrix4 transform2, T, R, S;

    MatrixUtil::ToTranslation(T, testvec);
    testrot.Invert();
    MatrixUtil::ToRotation(R, testrot);
    MatrixUtil::ToScaling(S, Vector3(1, 1, 1));

    transform2 = R * T;
    std::cout << "Transform2 = " << transform2 << "\n\n";
}

void RigidBody::ClearAccumulators()
{
    m_force_accum = Vector3::Zero();
    m_torque_accum = Vector3::Zero();
}

void RigidBody::CalculateDerivedData()
{
    m_orientation.Normalize();

    CalculateTransformMatrix(m_transform, m_position, m_orientation);
    TransformInertiaTensor(m_inverse_inertia_tensor_world, m_orientation,
        m_inverse_inertia_tensor, m_transform);
}

void RigidBody::Integrate(double dt)
{
    if (!m_is_awake) {
        return;
    }

    m_last_acceleration = m_acceleration + (m_force_accum * m_inverse_mass);

    Vector3 angular_acceleration = m_torque_accum * m_inverse_inertia_tensor_world;

    m_velocity += m_last_acceleration * dt;
    m_rotation += angular_acceleration * dt;

    m_velocity *= pow(m_linear_damping, dt);
    m_rotation *= pow(m_angular_damping, dt);

    m_position += m_velocity * dt;
    m_orientation += m_rotation * dt;

    CalculateDerivedData();
    ClearAccumulators();

    if (m_can_sleep) {
        // check if we can sleep
        double current_motion = m_velocity.Dot(m_velocity) + m_rotation.Dot(m_rotation);
        double bias = pow(0.5, dt);
        m_motion = bias * m_motion + (1.0 - bias) * current_motion;

        if (m_motion < SLEEP_EPSILON) {
            SetAwake(false);
        } else if (m_motion > 10.0 * SLEEP_EPSILON) {
            m_motion = 10.0 * SLEEP_EPSILON;
        }
    }
}
} // namespace apex