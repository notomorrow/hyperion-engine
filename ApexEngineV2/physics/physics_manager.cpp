#include "physics_manager.h"
#include "collision_list.h"
#include "../rendering/environment.h"

#include "btBulletDynamicsCommon.h"

#include <algorithm>

namespace hyperion {
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
    m_collision_configuration = new btDefaultCollisionConfiguration();
    m_dispatcher = new btCollisionDispatcher(m_collision_configuration);
    m_broadphase_interface = new btDbvtBroadphase();
    m_solver = new btSequentialImpulseConstraintSolver();
    m_dynamics_world = new btDiscreteDynamicsWorld(
        m_dispatcher,
        m_broadphase_interface,
        m_solver,
        m_collision_configuration
    );

    m_dynamics_world->setGravity(btVector3(
        Environment::GetInstance()->GetGravity().x,
        Environment::GetInstance()->GetGravity().y,
        Environment::GetInstance()->GetGravity().z
    ));
}

PhysicsManager::~PhysicsManager()
{
    // copy, as OnRemoved() may erase rigidbodies from m_bodies.
    std::vector<physics::RigidBody*> bodies(m_bodies);

    for (physics::RigidBody *body : bodies) {
        body->OnRemoved(); // do not delete here, as it is used by shared_ptrs as entity controls
    }

    delete m_dynamics_world;
    delete m_solver;
    delete m_broadphase_interface;
    delete m_dispatcher;
    delete m_collision_configuration;
}

void PhysicsManager::RegisterBody(physics::RigidBody *body)
{
    m_dynamics_world->addRigidBody(body->m_rigid_body);

    m_bodies.push_back(body);
}

void PhysicsManager::UnregisterBody(physics::RigidBody *body)
{
    auto it = std::find(m_bodies.begin(), m_bodies.end(), body);

    if (it != m_bodies.end()) {
        m_bodies.erase(it);
    }

    if (body->m_rigid_body != nullptr) {
        m_dynamics_world->removeRigidBody(body->m_rigid_body);
    }
}

void PhysicsManager::RunPhysics(double dt)
{
    m_dynamics_world->stepSimulation(dt);
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
} // namespace hyperion
