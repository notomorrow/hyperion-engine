#include "collision.h"
#include "rigid_body.h"
#include <cassert>

namespace apex {
namespace physics {
void Collision::ApplyVelocityChange(CollisionInfo &collision,
    std::array<Vector3, 2> &linear_change, std::array<Vector3, 2> &angular_change)
{
    std::array<Matrix3, 2> inverse_inertia_tensor;
    inverse_inertia_tensor[0] = collision.m_bodies[0]->GetInverseInertiaTensorWorld();
    if (collision.m_bodies[1] != nullptr) {
        inverse_inertia_tensor[1] = collision.m_bodies[1]->GetInverseInertiaTensorWorld();
    }

    Vector3 impulse_contact;
    if (collision.m_combined_material.GetFriction() == 0.0) {
        impulse_contact = CalculateFrictionlessImpulse(collision, inverse_inertia_tensor);
    } else {
        impulse_contact = CalculateFrictionImpulse(collision, inverse_inertia_tensor);
    }

    Vector3 impulse = impulse_contact * collision.m_contact_to_world;

    Vector3 impulsive_torque = collision.m_relative_contact_position[0];
    impulsive_torque.Cross(impulse);

    angular_change[0] = impulsive_torque * inverse_inertia_tensor[0];
    linear_change[0] = impulse * collision.m_bodies[0]->GetPhysicsMaterial().GetInverseMass();

    collision.m_bodies[0]->AddLinearVelocity(linear_change[0]);
    collision.m_bodies[0]->AddAngularVelocity(angular_change[0]);

    if (collision.m_bodies[1] != nullptr) {
        impulsive_torque = impulse;
        impulsive_torque.Cross(collision.m_relative_contact_position[1]);

        angular_change[1] = impulsive_torque * inverse_inertia_tensor[1];
        linear_change[1] = impulse * -collision.m_bodies[1]->GetPhysicsMaterial().GetInverseMass();

        collision.m_bodies[1]->AddLinearVelocity(linear_change[1]);
        collision.m_bodies[1]->AddAngularVelocity(angular_change[1]);
    }
}

void Collision::ApplyPositionChange(CollisionInfo &collision,
    std::array<Vector3, 2> &linear_change, std::array<Vector3, 2> &angular_change, double penetration)
{
    double total_inertia = 0.0;
    std::array<double, 2> linear_inertia;
    std::array<double, 2> angular_inertia;
    std::array<double, 2> angular_move;
    std::array<double, 2> linear_move;

    for (int i = 0; i < 2; i++) {
        if (collision.m_bodies[i] != nullptr) {
            Matrix3 inverse_inertia_tensor = collision.m_bodies[i]->GetInverseInertiaTensorWorld();

            Vector3 angular_inertia_world = collision.m_relative_contact_position[i];
            angular_inertia_world.Cross(collision.m_contact_normal);

            angular_inertia_world *= inverse_inertia_tensor;
            angular_inertia_world.Cross(collision.m_relative_contact_position[i]);
            angular_inertia[i] = angular_inertia_world.Dot(collision.m_contact_normal);

            linear_inertia[i] = collision.m_bodies[i]->GetPhysicsMaterial().GetInverseMass();
            total_inertia += linear_inertia[i] + angular_inertia[i];

        }
    }

    for (int i = 0; i < 2; i++) {
        if (collision.m_bodies[i] != nullptr) {
            double sign = (i == 0) ? 1 : -1;
            angular_move[i] = sign * penetration * (angular_inertia[i] / total_inertia);
            linear_move[i] = sign * penetration * (linear_inertia[i] / total_inertia);

            Vector3 proj = collision.m_relative_contact_position[i] +
                (collision.m_contact_normal * (-collision.m_relative_contact_position[i].Dot(collision.m_contact_normal)));

            double max_magnitude = COLLISION_ANGULAR_LIMIT * proj.Length();

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
                Vector3 target_angular_direction = collision.m_relative_contact_position[i];
                target_angular_direction.Cross(collision.m_contact_normal);
                Matrix3 inverse_inertia_tensor = collision.m_bodies[i]->GetInverseInertiaTensorWorld();

                angular_change[i] = (target_angular_direction * inverse_inertia_tensor) *
                    (angular_move[i] / angular_inertia[i]);
            }

            linear_change[i] = collision.m_contact_normal * linear_move[i];

            if (!collision.m_bodies[i]->IsStatic()) {
                Vector3 &pos = collision.m_bodies[i]->GetPosition();
                pos += collision.m_contact_normal * linear_move[i];

                Quaternion &rot = collision.m_bodies[i]->GetOrientation();
                rot += angular_change[i];
                rot.Normalize();

                if (!collision.m_bodies[i]->IsAwake()) {
                    // reflect changes on sleeping object
                    collision.m_bodies[i]->UpdateTransform();
                }
            }
        }
    }
}

void Collision::CalculateInternals(CollisionInfo &collision, double dt)
{
    if (collision.m_bodies[0] == nullptr) {
        SwapBodies(collision);
    }

    assert(collision.m_bodies[0] != nullptr);

    CalculateContactBasis(collision);

    collision.m_relative_contact_position[0] = collision.m_contact_point - collision.m_bodies[0]->GetPosition();
    if (collision.m_bodies[1] != nullptr) {
        collision.m_relative_contact_position[1] = collision.m_contact_point - collision.m_bodies[1]->GetPosition();
    }

    collision.m_contact_velocity = CalculateLocalVelocity(collision, 0, dt);
    if (collision.m_bodies[1] != nullptr) {
        collision.m_contact_velocity -= CalculateLocalVelocity(collision, 1, dt);
    }

    CalculateDesiredDeltaVelocity(collision, dt);
}

void Collision::SwapBodies(CollisionInfo &collision)
{
    collision.m_contact_normal *= -1.0f;
    std::swap(collision.m_bodies[0], collision.m_bodies[1]);
}

void Collision::MatchAwakeState(CollisionInfo &collision)
{
    if (collision.m_bodies[1] == nullptr) {
        return;
    }

    bool body0_awake = collision.m_bodies[0]->IsAwake();
    bool body1_awake = collision.m_bodies[1]->IsAwake();

    // only wake sleeping body
    if (body0_awake ^ body1_awake) {
        if (body0_awake) {
            collision.m_bodies[1]->SetAwake(true);
        } else {
            collision.m_bodies[0]->SetAwake(true);
        }
    }
}

Vector3 Collision::CalculateLocalVelocity(CollisionInfo &collision, 
    unsigned int body_index, double dt)
{
    Rigidbody *body = collision.m_bodies[body_index];

    Vector3 velocity(body->GetAngularVelocity());
    velocity.Cross(collision.m_relative_contact_position[body_index]);
    velocity += body->GetLinearVelocity();

    Matrix3 contact_to_world_transpose = collision.m_contact_to_world;
    contact_to_world_transpose.Transpose();

    Vector3 contact_velocity = velocity * contact_to_world_transpose;

    Vector3 accumulate_velocity = body->GetLastAcceleration() * dt;
    accumulate_velocity *= contact_to_world_transpose;
    accumulate_velocity.SetX(0);

    contact_velocity += accumulate_velocity;
    return contact_velocity;
}

void Collision::CalculateContactBasis(CollisionInfo &collision)
{
    std::array<Vector3, 2> contact_tangent;
    Vector3 contact_normal = collision.m_contact_normal;

    if (fabs(contact_normal.x) > fabs(contact_normal.y)) {
        const double s = 1.0 / sqrt(contact_normal.z * contact_normal.z +
            contact_normal.x * contact_normal.x);

        contact_tangent[0] = Vector3(contact_normal.z * s, 0.0, -contact_normal.x * s);
        contact_tangent[1] = Vector3(contact_normal.y * contact_tangent[0].x,
            contact_normal.z * contact_tangent[0].x - contact_normal.x * contact_tangent[0].z,
            -contact_normal.y * contact_tangent[0].x);
    } else {
        const double s = 1.0 / sqrt(contact_normal.z * contact_normal.z +
            contact_normal.y * contact_normal.y);

        contact_tangent[0] = Vector3(0.0, -contact_normal.z * s, contact_normal.y * 2);
        contact_tangent[1] = Vector3(contact_normal.y * contact_tangent[0].z - contact_normal.z * contact_tangent[0].y,
            -contact_normal.x * contact_tangent[0].z,
            contact_normal.x * contact_tangent[0].y);
    }

    float fv[] = {
        contact_normal.x, contact_tangent[0].x, contact_tangent[1].x,
        contact_normal.y, contact_tangent[0].y, contact_tangent[1].y,
        contact_normal.z, contact_tangent[0].z, contact_tangent[1].z
    };

    collision.m_contact_to_world = Matrix3(fv);
}

void Collision::CalculateDesiredDeltaVelocity(CollisionInfo &collision, double dt)
{
    double velocity_accumulate = 0.0;

    if (collision.m_bodies[0]->IsAwake()) {
        velocity_accumulate += (collision.m_bodies[0]->GetLastAcceleration() * dt).Dot(collision.m_contact_normal);
    }

    if (collision.m_bodies[1] != nullptr && collision.m_bodies[1]->IsAwake()) {
        velocity_accumulate -= (collision.m_bodies[1]->GetLastAcceleration() * dt).Dot(collision.m_contact_normal);
    }

    // limit restitution
    double res = collision.m_combined_material.GetRestitution();
    if (fabs(collision.m_contact_velocity.x) < COLLISION_VELOCITY_LIMIT) {
        res = 0.0;
    }

    collision.m_desired_delta_velocity = -collision.m_contact_velocity.x - res *
        (collision.m_contact_velocity.x - velocity_accumulate);
}

Vector3 Collision::CalculateFrictionlessImpulse(CollisionInfo &collision, 
    const std::array<Matrix3, 2> &inverse_inertia_tensor)
{
    Vector3 delta_velocity_world = collision.m_relative_contact_position[0];
    delta_velocity_world.Cross(collision.m_contact_normal);
    delta_velocity_world *= inverse_inertia_tensor[0];
    delta_velocity_world.Cross(collision.m_relative_contact_position[0]);

    double delta_velocity = delta_velocity_world.Dot(collision.m_contact_normal) +
        collision.m_bodies[0]->GetPhysicsMaterial().GetInverseMass();

    if (collision.m_bodies[1] != nullptr) {
        Vector3 delta_velocity_world = collision.m_relative_contact_position[1];
        delta_velocity_world.Cross(collision.m_contact_normal);
        delta_velocity_world *= inverse_inertia_tensor[1];
        delta_velocity_world.Cross(collision.m_relative_contact_position[1]);

        delta_velocity += delta_velocity_world.Dot(collision.m_contact_normal) +
            collision.m_bodies[1]->GetPhysicsMaterial().GetInverseMass();
    }

    return Vector3(collision.m_desired_delta_velocity / delta_velocity, 0.0, 0.0);
}

Vector3 Collision::CalculateFrictionImpulse(CollisionInfo &collision, 
    const std::array<Matrix3, 2> &inverse_inertia_tensor)
{
    double inverse_mass = collision.m_bodies[0]->GetPhysicsMaterial().GetInverseMass();

    float skew_symmetric[] = {
        0, -collision.m_relative_contact_position[0].z, collision.m_relative_contact_position[0].y,
        collision.m_relative_contact_position[0].z, 0, -collision.m_relative_contact_position[0].x,
        -collision.m_relative_contact_position[0].y, collision.m_relative_contact_position[0].x, 0
    };
    Matrix3 impulse_to_torque(skew_symmetric);
    Matrix3 delta_velocity_world_1 = impulse_to_torque;
    delta_velocity_world_1 *= inverse_inertia_tensor[0];
    delta_velocity_world_1 *= impulse_to_torque;
    delta_velocity_world_1 *= -1;

    if (collision.m_bodies[1] != nullptr) {
        float skew_symmetric[] = {
            0, -collision.m_relative_contact_position[1].z, collision.m_relative_contact_position[1].y,
            collision.m_relative_contact_position[1].z, 0, -collision.m_relative_contact_position[1].x,
            -collision.m_relative_contact_position[1].y, collision.m_relative_contact_position[1].x, 0
        };
        impulse_to_torque = Matrix3(skew_symmetric);

        Matrix3 delta_velocity_world_2 = impulse_to_torque;
        delta_velocity_world_2 *= inverse_inertia_tensor[1];
        delta_velocity_world_2 *= impulse_to_torque;
        delta_velocity_world_2 *= -1;

        delta_velocity_world_1 += delta_velocity_world_2;

        inverse_mass += collision.m_bodies[1]->GetPhysicsMaterial().GetInverseMass();
    }

    Matrix3 delta_velocity = collision.m_contact_to_world;
    delta_velocity.Transpose();
    delta_velocity *= delta_velocity_world_1;
    delta_velocity *= collision.m_contact_to_world;

    delta_velocity(0, 0) += inverse_mass;
    delta_velocity(1, 1) += inverse_mass;
    delta_velocity(2, 2) += inverse_mass;

    Matrix3 impulse_matrix = delta_velocity;
    impulse_matrix.Invert();

    Vector3 kill_velocity(collision.m_desired_delta_velocity, 
        -collision.m_contact_velocity.y, -collision.m_contact_velocity.z);
    Vector3 impulse_contact = kill_velocity * impulse_matrix;

    double planar_impulse = sqrt(impulse_contact.y * impulse_contact.y +
        impulse_contact.z * impulse_contact.z);

    if (planar_impulse > impulse_contact.x * collision.m_combined_material.GetFriction()) {
        impulse_contact.y /= planar_impulse;
        impulse_contact.z /= planar_impulse;

        impulse_contact.x =
            delta_velocity(0, 0) +
            delta_velocity(0, 1) * collision.m_combined_material.GetFriction() * impulse_contact.y +
            delta_velocity(0, 2) * collision.m_combined_material.GetFriction() * impulse_contact.z;

        impulse_contact.x = collision.m_desired_delta_velocity / impulse_contact.x;
        impulse_contact.y *= collision.m_combined_material.GetFriction() * impulse_contact.x;
        impulse_contact.z *= collision.m_combined_material.GetFriction() * impulse_contact.x;
    }

    return impulse_contact;
}
} // namespace physics
} // namespace apex