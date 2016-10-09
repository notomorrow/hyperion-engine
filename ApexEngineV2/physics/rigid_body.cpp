#include "rigid_body.h"
#include "../math/matrix_util.h"

namespace apex {
namespace physics {
static Matrix4 CalculateTransformMatrix(const Vector3 &position, Quaternion orientation)
{
    orientation.Invert();

    Matrix4 R, T;

    MatrixUtil::ToRotation(R, orientation);
    MatrixUtil::ToTranslation(T, position);

    return R * T;
}

static Matrix3 CalculateInverseInertiaWorldMatrix(const Matrix3 &iit_body, const Matrix4 &transform)
{
    auto t4 = transform(0, 0) * iit_body(0, 0) +
        transform(0, 1) * iit_body(1, 0) +
        transform(0, 2) * iit_body(2, 0);
    auto t9 = transform(0, 0) * iit_body(0, 1) +
        transform(0, 1) * iit_body(1, 1) +
        transform(0, 2) * iit_body(2, 1);
    auto t14 = transform(0, 0) * iit_body(0, 2) +
        transform(0, 1)* iit_body(1, 2) +
        transform(0, 2) * iit_body(2, 2);
    auto t28 = transform(1, 0) * iit_body(0, 0) +
        transform(1, 1) * iit_body(1, 0) +
        transform(1, 2) * iit_body(2, 0);
    auto t33 = transform(1, 0) * iit_body(0, 1) +
        transform(1, 1) * iit_body(1, 1) +
        transform(1, 2) * iit_body(2, 1);
    auto t38 = transform(1, 0) * iit_body(0, 2) +
        transform(1, 1) * iit_body(1, 2) +
        transform(1, 2) * iit_body(2, 2);
    auto t52 = transform(2, 0) * iit_body(0, 0) +
        transform(2, 1) * iit_body(1, 0) +
        transform(2, 2) * iit_body(2, 0);
    auto t57 = transform(2, 0) * iit_body(0, 1) +
        transform(2, 1) * iit_body(1, 1) +
        transform(2, 2) * iit_body(2, 1);
    auto t62 = transform(2, 0) * iit_body(0, 2) +
        transform(2, 1) * iit_body(1, 2) +
        transform(2, 2) * iit_body(2, 2);

    Matrix3 iit_world;

    iit_world.values[0] = t4 * transform(0, 0) +
        t9 * transform(0, 1) +
        t14 * transform(0, 2);
    iit_world.values[1] = t4 * transform(1, 0) +
        t9 * transform(1, 1) +
        t14 * transform(1, 2);
    iit_world.values[2] = t4 * transform(2, 0) +
        t9 * transform(2, 1) +
        t14 * transform(2, 2);
    iit_world.values[3] = t28 * transform(0, 0) +
        t33 * transform(0, 1) +
        t38 * transform(0, 2);
    iit_world.values[4] = t28 * transform(1, 0) +
        t33 * transform(1, 1) +
        t38 * transform(1, 2);
    iit_world.values[5] = t28 * transform(2, 0) +
        t33 * transform(2, 1) +
        t38 * transform(2, 2);
    iit_world.values[6] = t52*transform(0, 0) +
        t57 * transform(0, 1) +
        t62 * transform(0, 2);
    iit_world.values[7] = t52 * transform(1, 0) +
        t57 * transform(1, 1) +
        t62 * transform(1, 2);
    iit_world.values[8] = t52 * transform(2, 0) +
        t57 * transform(2, 1) +
        t62 * transform(2, 2);

    return iit_world;
}

RigidBody::RigidBody(std::shared_ptr<PhysicsShape> shape, PhysicsMaterial material)
    : EntityControl(60.0),
      m_shape(shape), 
      m_material(material), 
      m_awake(true)
{
}

void RigidBody::UpdateTransform()
{
    m_orientation.Normalize();
    m_transform = CalculateTransformMatrix(m_position, m_orientation);
    m_inv_inertia_tensor_world = CalculateInverseInertiaWorldMatrix(m_inv_inertia_tensor, m_transform);

    m_shape->m_transform = m_transform;
}

void RigidBody::Integrate(double dt)
{
    if (m_awake) {
        m_last_acceleration = m_acceleration + (m_force_accum * m_material.GetInverseMass());

        m_linear_velocity += m_last_acceleration * dt;
        m_linear_velocity *= pow(m_material.GetLinearDamping(), dt);
        m_position += m_linear_velocity * dt;

        Vector3 angular_acceleration = m_torque_accum * m_inv_inertia_tensor_world;
        m_angular_velocity += angular_acceleration * dt;
        m_angular_velocity *= pow(m_material.GetAngularDamping(), dt);
        m_orientation += m_angular_velocity * dt;

        UpdateTransform();

        // reset accumulators
        m_force_accum = Vector3::Zero();
        m_torque_accum = Vector3::Zero();
    }
}

void RigidBody::OnAdded()
{
}

void RigidBody::OnRemoved()
{
}

void RigidBody::OnUpdate(double dt)
{
    Quaternion tmp(m_orientation);
    tmp.Invert();
    parent->SetLocalRotation(tmp);
    parent->SetLocalTranslation(m_position);
}

} // namespace physics
} // namespace apex