#ifndef PHYSICS_MANAGER_H
#define PHYSICS_MANAGER_H

#include "physics_object.h"

#include <vector>

namespace apex {
class PhysicsManager {
public:
    static PhysicsManager *GetInstance();

    PhysicsManager();

    // add the PhysicsObject to be collidable with other objects in the world
    void AddPhysicsObject(PhysicsObject *object);
    void RemovePhysicsObject(PhysicsObject *object);

    // test collisions between all colliders
    void CheckCollisions();

private:
    static PhysicsManager *instance;

    std::vector<PhysicsObject*> objects;
};
}

#endif