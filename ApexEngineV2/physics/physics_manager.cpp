#include "physics_manager.h"
#include "collision_list.h"
#include "../rendering/environment.h"

#include <algorithm>

namespace apex {
PhysicsManager *PhysicsManager::instance = nullptr;

PhysicsManager *PhysicsManager::GetInstance()
{
    if (instance == nullptr) {
        instance = new PhysicsManager();
    }
    return instance;
}

PhysicsManager::PhysicsManager()
{
}

PhysicsManager::~PhysicsManager()
{
}

void PhysicsManager::RegisterBody(std::shared_ptr<physics::RigidBody> body)
{
    m_bodies.push_back(body);
}

void PhysicsManager::RunPhysics(double dt)
{
    // gather collisions
    std::vector<physics::CollisionInfo> collisions;

    for (size_t i = 0; i < m_bodies.size(); i++) {
        physics::RigidBody *a = m_bodies[i].get();

        if (a == nullptr) {
            continue;
        }

        for (size_t j = i + 1; j < m_bodies.size(); j++) {
            physics::RigidBody *b = m_bodies[j].get();
            if (b == nullptr || b == a) {
                // skip duplicates
                continue;
            }

            physics::PhysicsShape *b_shape = b->GetPhysicsShape().get();

            // collision info for these two bodies will be stored in this object
            physics::CollisionList list;

            switch (b->GetPhysicsShape()->GetType()) {
            case physics::PhysicsShape_box:
                if (a->GetPhysicsShape()->CollidesWith(static_cast<physics::BoxPhysicsShape*>(b_shape), list)) {
                    for (physics::CollisionInfo &info : list.m_collisions) {
                        // add more info (bodies involved)
                        info.m_bodies = { a, b };
                        // combine materials of each object
                        info.m_combined_material.SetFriction(std::min(
                            a->GetPhysicsMaterial().GetFriction(),
                            b->GetPhysicsMaterial().GetFriction()
                        ));
                        info.m_combined_material.SetRestitution(std::min(
                            a->GetPhysicsMaterial().GetRestitution(),
                            b->GetPhysicsMaterial().GetRestitution()
                        ));
                        collisions.push_back(info);
                    }
                }
                break;
            case physics::PhysicsShape_sphere:
                if (a->GetPhysicsShape()->CollidesWith(static_cast<physics::SpherePhysicsShape*>(b_shape), list)) {
                    for (physics::CollisionInfo &info : list.m_collisions) {
                        // add more info (bodies involved)
                        info.m_bodies = { a, b };
                        // combine materials of each object
                        info.m_combined_material.SetFriction(std::min(
                            a->GetPhysicsMaterial().GetFriction(),
                            b->GetPhysicsMaterial().GetFriction()
                        ));
                        info.m_combined_material.SetRestitution(std::min(
                            a->GetPhysicsMaterial().GetRestitution(),
                            b->GetPhysicsMaterial().GetRestitution()
                        ));
                        collisions.push_back(info);
                    }
                }
                break;
            case physics::PhysicsShape_plane:
                if (a->GetPhysicsShape()->CollidesWith(static_cast<physics::PlanePhysicsShape*>(b_shape), list)) {
                    for (physics::CollisionInfo &info : list.m_collisions) {
                        // add more info (bodies involved)
                        info.m_bodies = { a, b };
                        // combine materials of each object
                        info.m_combined_material.SetFriction(std::min(
                            a->GetPhysicsMaterial().GetFriction(),
                            b->GetPhysicsMaterial().GetFriction()
                        ));
                        info.m_combined_material.SetRestitution(std::min(
                            a->GetPhysicsMaterial().GetRestitution(),
                            b->GetPhysicsMaterial().GetRestitution()
                        ));
                        collisions.push_back(info);
                    }
                }
                break;
            }
        }
    }

    // update internal data of collisions
    UpdateInternals(collisions, dt);
    // update positions of collisions
    UpdatePositions(collisions, dt);
    // update velocities of collisions
    UpdateVelocities(collisions, dt);

    for (auto &body : m_bodies) {
        if (body->IsAwake() && !body->IsStatic()) {
            body->ApplyForce(Environment::GetInstance()->GetGravity() * body->GetPhysicsMaterial().GetMass() * dt);
            body->Integrate(dt);
        }
    }
}

void PhysicsManager::UpdateInternals(std::vector<physics::CollisionInfo> &collisions, double dt)
{
    for (physics::CollisionInfo &item : collisions) {
        physics::Collision::CalculateInternals(item, dt);
    }
}

void PhysicsManager::UpdateVelocities(std::vector<physics::CollisionInfo> &collisions, double dt)
{
    Vector3 delta_velocity;
    std::array<Vector3, 2> linear_change;
    std::array<Vector3, 2> angular_change;

    const unsigned int num_collisions = collisions.size();
    const unsigned int num_iterations = num_collisions * 4;

    for (unsigned int iteration = 0; iteration < num_iterations; iteration++) {
        double max = VELOCITY_EPSILON;

        unsigned int index = num_collisions;
        for (unsigned int i = 0; i < num_collisions; i++) {
            auto &collision = collisions[i];
            if (collision.m_desired_delta_velocity > max) {
                max = collision.m_desired_delta_velocity;
                index = i;
            }
        }

        if (index == num_collisions) {
            break;
        }

        physics::Collision::MatchAwakeState(collisions[index]);
        physics::Collision::ApplyVelocityChange(collisions[index], linear_change, angular_change);

        for (unsigned int i = 0; i < num_collisions; i++) {
            for (unsigned int b = 0; b < 2; b++) {
                if (collisions[i].m_bodies[b] != nullptr) {
                    for (unsigned int d = 0; d < 2; d++) {
                        if (collisions[i].m_bodies[b] == collisions[index].m_bodies[d]) {
                            delta_velocity = linear_change[d];
                            Vector3 cross_product = angular_change[d];
                            cross_product.Cross(collisions[i].m_relative_contact_position[b]);
                            delta_velocity += cross_product;

                            Matrix3 contact_transpose = collisions[i].m_contact_to_world;
                            contact_transpose.Transpose();

                            collisions[i].m_contact_velocity += (delta_velocity * contact_transpose) * (b ? -1 : 1);
                            physics::Collision::CalculateDesiredDeltaVelocity(collisions[i], dt);
                        }
                    }
                }
            }
        }
    }
}

void PhysicsManager::UpdatePositions(std::vector<physics::CollisionInfo> &collisions, double dt)
{
    Vector3 delta_position;
    std::array<Vector3, 2> linear_change;
    std::array<Vector3, 2> angular_change;

    const unsigned int num_collisions = collisions.size();
    const unsigned int num_iterations = num_collisions * 4;

    for (unsigned int iteration = 0; iteration < num_iterations; iteration++) {
        double max = POSITION_EPSILON;

        unsigned int index = num_collisions;
        for (unsigned int i = 0; i < num_collisions; i++) {
            if (collisions[i].m_contact_penetration > max) {
                max = collisions[i].m_contact_penetration;
                index = i;
            }
        }

        if (index == num_collisions) {
            break;
        }

        physics::Collision::MatchAwakeState(collisions[index]);
        physics::Collision::ApplyPositionChange(collisions[index], linear_change, angular_change, max);

        for (unsigned int i = 0; i < num_collisions; i++) {
            for (unsigned int b = 0; b < 2; b++) {
                if (collisions[i].m_bodies[b] != nullptr) {
                    for (unsigned int d = 0; d < 2; d++) {
                        if (collisions[i].m_bodies[b] == collisions[index].m_bodies[d]) {
                            delta_position = linear_change[d];
                            Vector3 cross_product = angular_change[d];
                            cross_product.Cross(collisions[i].m_relative_contact_position[b]);
                            delta_position += cross_product;

                            collisions[i].m_contact_penetration += delta_position.Dot(collisions[i].m_contact_normal) *
                                (b ? 1 : -1);
                        }
                    }
                }
            }
        }
    }
}
} // namespace apex
