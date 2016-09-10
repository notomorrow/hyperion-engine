#include "contact.h"
#include <cassert>

namespace apex {
Contact::Contact()
    : m_contact_penetration(0.0)
{
    // initialize all bodies to nullptr
    m_bodies = { nullptr, nullptr };
}

void Contact::SetBodyData(RigidBody *one, RigidBody *two,
    double friction, double restitution)
{
    m_bodies[0] = one;
    m_bodies[1] = two;
    m_friction = friction;
    m_restitution = restitution;
}

void Contact::CalculateInternals(double dt)
{
    if (m_bodies[0] == nullptr) {
        SwapBodies();
    }

    assert(m_bodies[0] != nullptr);

    CalculateContactBasis();

    m_relative_contact_position[0] = m_contact_point - m_bodies[0]->position;
    if (m_bodies[1] != nullptr) {
        m_relative_contact_position[1] = m_contact_point - m_bodies[1]->position;
    }

    m_contact_velocity = CalculateLocalVelocity(0, dt);
    if (m_bodies[1] != nullptr) {
        m_contact_velocity -= CalculateLocalVelocity(1, dt);
    }

    CalculateDesiredDeltaVelocity(dt);
}

void Contact::SwapBodies()
{
    m_contact_normal *= -1;
    std::swap(m_bodies[0], m_bodies[1]);
}

void Contact::MatchAwakeState()
{
    if (m_bodies[1] == nullptr) {
        return;
    }

    bool body0_awake = m_bodies[0]->IsAwake();
    bool body1_awake = m_bodies[1]->IsAwake();

    // only wake sleeping body
    if (body0_awake ^ body1_awake) {
        if (body0_awake) {
            m_bodies[1]->SetAwake(true);
        } else {
            m_bodies[0]->SetAwake(true);
        }
    }
}

void Contact::CalculateDesiredDeltaVelocity(double dt)
{
    double velocity_accumulate = 0.0;

    if (m_bodies[0]->IsAwake()) {
        velocity_accumulate += (m_bodies[0]->last_acceleration * dt).Dot(m_contact_normal);
    }

    if (m_bodies[1] != nullptr && m_bodies[1]->IsAwake()) {
        velocity_accumulate -= (m_bodies[1]->last_acceleration * dt).Dot(m_contact_normal);
    }

    // limit restitution
    double res = m_restitution;
    if (fabs(m_contact_velocity.x) < CONTACT_VELOCITY_LIMIT) {
        res = 0.0;
    }

    m_desired_delta_velocity = -m_contact_velocity.x - res * (m_contact_velocity.x - velocity_accumulate);
}

Vector3 Contact::CalculateLocalVelocity(unsigned int body_index, double dt)
{
    RigidBody *body = m_bodies[body_index];

    Vector3 rot = body->rotation;
    Vector3 velocity = rot.Cross(m_relative_contact_position[body_index]);
    velocity += body->velocity;

    Matrix3 contact_to_world_transpose = m_contact_to_world;
    contact_to_world_transpose.Transpose();
    
    Vector3 contact_velocity = velocity * contact_to_world_transpose;
    
    Vector3 accumulate_velocity = body->last_acceleration * dt;
    accumulate_velocity *= contact_to_world_transpose;
    accumulate_velocity.SetX(0);

    contact_velocity += accumulate_velocity;

    return contact_velocity;
}

void Contact::CalculateContactBasis()
{
    std::array<Vector3, 2> contact_tangent;

    if (abs(m_contact_normal.x) > abs(m_contact_normal.y)) {
        const double s = 1.0 / sqrt(m_contact_normal.z * m_contact_normal.z + 
            m_contact_normal.x * m_contact_normal.x);

        contact_tangent[0] = Vector3(m_contact_normal.z * s, 0.0, -m_contact_normal.x * s);
        contact_tangent[1] = Vector3(m_contact_normal.y * contact_tangent[0].x,
            m_contact_normal.z * contact_tangent[0].x - m_contact_normal.x * contact_tangent[0].z,
            -m_contact_normal.y * contact_tangent[0].x);
    } else {
        const double s = 1.0 / sqrt(m_contact_normal.z * m_contact_normal.z +
            m_contact_normal.y * m_contact_normal.y);

        contact_tangent[0] = Vector3(0.0, -m_contact_normal.z * s, m_contact_normal.y * 2);
        contact_tangent[1] = Vector3(m_contact_normal.y * contact_tangent[0].z - m_contact_normal.z * contact_tangent[0].y,
            -m_contact_normal.x * contact_tangent[0].z,
            m_contact_normal.x * contact_tangent[0].y);
    }

    float fv[] = {
        m_contact_normal.x, contact_tangent[0].x, contact_tangent[1].x,
        m_contact_normal.y, contact_tangent[0].y, contact_tangent[1].y,
        m_contact_normal.z, contact_tangent[0].z, contact_tangent[1].z
    };
    m_contact_to_world = fv;
}

void Contact::ApplyImpulse(const Vector3 &impulse, RigidBody *body,
    Vector3 &out_velocity_change, Vector3 &out_rotation_change)
{

}

void Contact::ApplyVelocityChange(std::array<Vector3, 2> &velocity_change, std::array<Vector3, 2> &rotation_change)
{
    std::array<Matrix3, 2> inverse_inertia_tensor;
    inverse_inertia_tensor[0] = m_bodies[0]->GetInverseInertiaTensorWorld();
    if (m_bodies[1] != nullptr) {
        inverse_inertia_tensor[1] = m_bodies[1]->GetInverseInertiaTensorWorld();
    }

    Vector3 impulse_contact;

    if (m_friction == 0.0) {
        impulse_contact = CalculateFrictionlessImpulse(inverse_inertia_tensor);
    } else {
        impulse_contact = CalculateFrictionImpulse(inverse_inertia_tensor);
    }

    Vector3 impulse = impulse_contact * m_contact_to_world;

    auto relative_contact_position = m_relative_contact_position;
    Vector3 impulsive_torque = relative_contact_position[0].Cross(impulse);
    rotation_change[0] = impulsive_torque * inverse_inertia_tensor[0];
    velocity_change[0] = Vector3::Zero();
    velocity_change[0] += impulse * m_bodies[0]->GetInverseMass();

    m_bodies[0]->velocity += velocity_change[0];
    m_bodies[0]->rotation += rotation_change[0];

    if (m_bodies[1] != nullptr) {
        impulsive_torque = impulse;
        impulsive_torque.Cross(relative_contact_position[1]);

        rotation_change[1] = impulsive_torque * inverse_inertia_tensor[1];
        velocity_change[1] = Vector3::Zero();
        velocity_change[1] += impulse * -m_bodies[1]->GetInverseMass();

        m_bodies[1]->velocity += velocity_change[1];
        m_bodies[1]->rotation += rotation_change[1];
    }
}

void Contact::ApplyPositionChange(std::array<Vector3, 2> &linear_change,
    std::array<Vector3, 2> &angular_change, double penetration)
{
    double total_inertia = 0.0;
    std::array<double, 2> linear_inertia;
    std::array<double, 2> angular_inertia;
    std::array<double, 2> angular_move;
    std::array<double, 2> linear_move;

    for (int i = 0; i < 2; i++) {
        if (m_bodies[i] != nullptr) {
            Matrix3 inverse_inertia_tensor = m_bodies[i]->GetInverseInertiaTensorWorld();

            Vector3 angular_inertia_world = m_relative_contact_position[i];
            angular_inertia_world.Cross(m_contact_normal);

            angular_inertia_world *= inverse_inertia_tensor;
            angular_inertia_world.Cross(m_relative_contact_position[i]);
            angular_inertia[i] = angular_inertia_world.Dot(m_contact_normal);

            linear_inertia[i] = m_bodies[i]->GetInverseMass();
            total_inertia += linear_inertia[i] + angular_inertia[i];

        }
    }

    for (int i = 0; i < 2; i++) {
        if (m_bodies[i] != nullptr) {
            double sign = (i == 0) ? 1 : -1;
            angular_move[i] = sign * penetration * (angular_inertia[i] / total_inertia);
            linear_move[i] = sign * penetration * (linear_inertia[i] / total_inertia);

            Vector3 proj = m_relative_contact_position[i] +
                (m_contact_normal * -m_relative_contact_position[i].Dot(m_contact_normal));

            double max_magnitude = CONTACT_ANGULAR_LIMIT * proj.Length();

            if (angular_move[i] < -max_magnitude) {
                double total_move = angular_move[i] + linear_move[i];
                angular_move[i] = -max_magnitude;
                linear_move[i] = total_move - angular_move[i];
            } else if (angular_move[i] > max_magnitude) {
                double total_move = angular_move[i] + linear_move[i];
                angular_move[i] = max_magnitude;
                linear_move[i] = total_move - angular_move[i];
            }

            if (angular_move[i] == 0) {
                angular_change[i] = Vector3::Zero();
            } else {
                Vector3 target_angular_direction = m_relative_contact_position[i] * m_contact_normal;
                Matrix3 inverse_inertia_tensor = m_bodies[i]->GetInverseInertiaTensorWorld();

                angular_change[i] = (target_angular_direction * inverse_inertia_tensor) *
                    (angular_move[i] / angular_inertia[i]);
            }

            linear_change[i] = m_contact_normal * linear_move[i];

            if (m_bodies[i]->HasFiniteMass()) {
                Vector3 &pos = m_bodies[i]->position;
                pos += m_contact_normal * linear_move[i];

                Quaternion &rot = m_bodies[i]->orientation;
                rot += angular_change[i];

                if (!m_bodies[i]->IsAwake()) {
                    m_bodies[i]->CalculateDerivedData();
                }
            }
        }
    }
}

Vector3 Contact::CalculateFrictionlessImpulse(const std::array<Matrix3, 2> &inverse_inertia_tensor)
{
    Vector3 delta_velocity_world = m_relative_contact_position[0];
    delta_velocity_world.Cross(m_contact_normal);
    delta_velocity_world *= inverse_inertia_tensor[0];
    delta_velocity_world.Cross(m_relative_contact_position[0]);

    double delta_velocity = delta_velocity_world.Dot(m_contact_normal) + 
        m_bodies[0]->GetInverseMass();

    if (m_bodies[1] != nullptr) {
        Vector3 delta_velocity_world = m_relative_contact_position[1];
        delta_velocity_world.Cross(m_contact_normal);
        delta_velocity_world *= inverse_inertia_tensor[1];
        delta_velocity_world.Cross(m_relative_contact_position[1]);

        delta_velocity += delta_velocity_world.Dot(m_contact_normal) + 
            m_bodies[1]->GetInverseMass();
    }

    return Vector3(m_desired_delta_velocity / delta_velocity, 0.0, 0.0);
}

Vector3 Contact::CalculateFrictionImpulse(const std::array<Matrix3, 2> &inverse_inertia_tensor)
{
    Vector3 impulse_contact;
    double inverse_mass = m_bodies[0]->GetInverseMass();

    float skew_symmetric[] = {
        0, -m_relative_contact_position[0].z, m_relative_contact_position[0].y,
        m_relative_contact_position[0].z, 0, -m_relative_contact_position[0].x,
        -m_relative_contact_position[0].y, m_relative_contact_position[0].x, 0
    };
    Matrix3 impulse_to_torque = skew_symmetric;

    Matrix3 delta_velocity_world_1 = impulse_to_torque;
    delta_velocity_world_1 *= inverse_inertia_tensor[0];
    delta_velocity_world_1 *= impulse_to_torque;
    delta_velocity_world_1 *= -1;

    if (m_bodies[1] != nullptr) {
        float skew_symmetric[] = {
            0, -m_relative_contact_position[1].z, m_relative_contact_position[1].y,
            m_relative_contact_position[1].z, 0, -m_relative_contact_position[1].x,
            -m_relative_contact_position[1].y, m_relative_contact_position[1].x, 0
        };
        impulse_to_torque = skew_symmetric;

        Matrix3 delta_velocity_world_2 = impulse_to_torque;
        delta_velocity_world_2 *= inverse_inertia_tensor[1];
        delta_velocity_world_2 *= impulse_to_torque;
        delta_velocity_world_2 *= -1;

        delta_velocity_world_1 += delta_velocity_world_2;

        inverse_mass += m_bodies[1]->GetInverseMass();
    }

    Matrix3 delta_velocity = m_contact_to_world;
    delta_velocity.Transpose();
    delta_velocity *= delta_velocity_world_1;
    delta_velocity *= m_contact_to_world;

    delta_velocity(0, 0) += inverse_mass;
    delta_velocity(1, 0) += inverse_mass;
    delta_velocity(2, 0) += inverse_mass;

    Matrix3 impulse_matrix = delta_velocity;
    impulse_matrix.Invert();

    Vector3 kill_velocity(m_desired_delta_velocity, -m_contact_velocity.y, -m_contact_velocity.z);
    impulse_contact = kill_velocity * impulse_matrix;

    double planar_impulse = sqrt(impulse_contact.y * impulse_contact.y +
        impulse_contact.z * impulse_contact.z);

    if (planar_impulse > impulse_contact.x * m_friction) {
        impulse_contact.y /= planar_impulse;
        impulse_contact.z /= planar_impulse;

        impulse_contact.x = delta_velocity(0, 0) +
            delta_velocity(0, 1) * m_friction * impulse_contact.y +
            delta_velocity(0, 2) * m_friction * impulse_contact.z;
        
        impulse_contact.x = m_desired_delta_velocity / impulse_contact.x;
        impulse_contact.y *= m_friction * impulse_contact.x;
        impulse_contact.z *= m_friction * impulse_contact.x;
    }

    return impulse_contact;
}
} // namespace apex