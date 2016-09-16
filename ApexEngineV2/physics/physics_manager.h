#ifndef PHYSICS_MANAGER_H
#define PHYSICS_MANAGER_H

#include "physics_def.h"

#if EXPERIMENTAL_PHYSICS

#define VELOCITY_EPSILON 0.001
#define POSITION_EPSILON 0.003

#include "physics2/rigid_body2.h"
#include "physics2/collision.h"
#include "physics2/box_physics_shape.h"
#include "physics2/sphere_physics_shape.h"
#include "physics2/plane_physics_shape.h"
#else
#include "rigid_body.h"
#include "contact.h"
#include "contact_generator.h"
#include "contact_resolver.h"
#include "collision_data.h"
#endif

#include <vector>
#include <memory>

namespace apex {
class PhysicsManager {
public:
    static PhysicsManager *GetInstance();

    PhysicsManager();
    ~PhysicsManager();

    void RegisterBody(std::shared_ptr<RIGID_BODY> body);
    void ResetCollisions();
    void DetectCollisions();
    void RunPhysics(double dt);
#if EXPERIMENTAL_PHYSICS
    void UpdateInternals(std::vector<physics::CollisionInfo> &collisions, double dt);
    void UpdateVelocities(std::vector<physics::CollisionInfo> &collisions, double dt);
    void UpdatePositions(std::vector<physics::CollisionInfo> &collisions, double dt);
#endif

#if !EXPERIMENTAL_PHYSICS
    ContactResolver *resolver;
    CollisionData collision_data;
#endif

private:
    static PhysicsManager *instance;
    std::vector<std::shared_ptr<RIGID_BODY>> m_bodies;
};
}

#endif