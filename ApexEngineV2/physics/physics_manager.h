#ifndef PHYSICS_MANAGER_H
#define PHYSICS_MANAGER_H

#include "physics_object.h"
#include "rigid_body.h"
#include "contact.h"
#include "contact_generator.h"
#include "contact_resolver.h"

#include <vector>
#include <memory>

namespace apex {
class PhysicsManager {
public:
    static PhysicsManager *GetInstance();

    PhysicsManager();
    ~PhysicsManager();

    void RegisterBody(std::shared_ptr<RigidBody> body);

    void Begin();
    unsigned int GenerateContacts();
    void RunPhysics(double dt);

    ContactResolver *resolver;

    /*
    // add the PhysicsObject to be collidable with other objects in the world
    void AddPhysicsObject(PhysicsObject *object);
    void RemovePhysicsObject(PhysicsObject *object);

    // test collisions between all colliders
    void CheckCollisions();*/

private:
    static PhysicsManager *instance;

    std::vector<std::shared_ptr<RigidBody>> m_bodies;
    std::vector<std::shared_ptr<ContactGenerator>> m_contact_generators;

    std::array<Contact, MAX_CONTACTS> contacts;
    unsigned int contact_index = 0;
    //std::vector<PhysicsObject*> objects;
};
}

#endif