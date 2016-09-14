#include "physics_manager.h"
#include "shapes/aabb_physics_object.h"

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
    resolver = new ContactResolver(0);

    collision_data.m_friction = 0.8;
    collision_data.m_restitution = 0.2;
    collision_data.m_tolerance = 0.1;
    collision_data.Reset();
}

PhysicsManager::~PhysicsManager()
{
    delete resolver;
}

void PhysicsManager::RegisterBody(std::shared_ptr<RigidBody> body)
{
    m_bodies.push_back(body);
}

void PhysicsManager::ResetCollisions()
{
    collision_data.Reset();
}

void PhysicsManager::RunPhysics(double dt)
{
    resolver->SetNumIterations(MAX_CONTACTS * 8);
    resolver->ResolveContacts(collision_data.m_contacts, collision_data.m_contact_count, dt);

    for (auto &&body : m_bodies) {
        // apply gravity to body
        body->ApplyForce(Vector3(0, -10, 0) * body->GetMass());
        body->Integrate(dt);
    }
}

/*void PhysicsManager::AddPhysicsObject(PhysicsObject *object)
{
    objects.push_back(object);
}

void PhysicsManager::RemovePhysicsObject(PhysicsObject *object)
{
    objects.erase(std::remove(objects.begin(), objects.end(), object), objects.end());
}

void PhysicsManager::CheckCollisions()
{
    for (size_t i = 0; i < objects.size(); i++) {
        PhysicsObject *a = objects[i];
        if (a == nullptr) {
            throw "a was nullptr";
        }

        a->grounded = false; // set flag to false, will be detected in loop

        // Use rays to check to see if grounded
        Ray ray;
        ray.position = a->position;
        ray.direction = Vector3(0, -1, 0);
        ray.direction.Normalize();

        Vector3 ray_hitpoint;

        for (size_t j = 0; j < objects.size(); j++) {
            PhysicsObject *b = objects[j];

            if (b == nullptr) {
                throw "b was nullptr";
            }

            if (a != b) {

                // Check to see if 'b' is below 'a'
                if (b->velocity.Length() <= 0.4 && b->RayTest(ray, ray_hitpoint)) {
                    Vector3 diff = a->position - ray_hitpoint;
                    // if within a threshold, ground 'a'
                    if (diff.y <= 0.3) {
                        a->grounded = true;
                        a->velocity = 0;
                        a->acceleration = 0;
                        a->force = 0;
                        continue;
                    } else {
                        //if (diff.Length() < a->ground_diff.Length()) {
                        //    a->grounded = false;
                        //}
                    }
                }

                // check collision between a and b
                Vector3 contact_normal;
                float contact_distance = 0.0;

                switch (b->shape) {
                case (PhysicsShape::AABB_physics_shape):
                    if (a->CheckCollision(static_cast<AABBPhysicsObject*>(b),
                        contact_normal, contact_distance)) {
                        Vector3 relative_velocity = b->velocity - a->velocity;
                        float contact_velocity = relative_velocity.Dot(contact_normal);

                        if (contact_velocity > 0.0) {
                            continue;
                        }

                        double a_inv_mass = a->mass == 0.0 ? 0.0 : 1.0 / a->mass;
                        double b_inv_mass = b->mass == 0.0 ? 0.0 : 1.0 / b->mass;

                        // Calculate restitution
                        double e = std::min(a->restitution, b->restitution);

                        // Calculate impulse
                        double j = -(1.0f + e) * contact_velocity;
                        j /= a_inv_mass + b_inv_mass;

                        // Apply impulse
                        float mass_sum = a->mass + b->mass;
                        if (mass_sum == 0.0) {
                            mass_sum = 1.0;
                        }

                        Vector3 impulse = contact_normal * j;
                        a->velocity -= impulse * a_inv_mass;
                        b->velocity += impulse * b_inv_mass;

                        a->force -= contact_normal * 2;
                        b->force += contact_normal * 2;

                        std::cout << "\"" << a->tag << "\" collided with \"" << b->tag << "\"\n";
                        std::cout << "impulse = " << impulse << "\n";
                        std::cout << "contact normal = " << contact_normal << "\n";
                        std::cout << "contact distance = " << contact_distance << "\n\n";


                    }
                                                       break;
                default:
                    throw "Unknown physics shape";
                }
            }
        }
    }
}*/
} // namespace apex