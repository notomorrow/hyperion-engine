#ifndef APEX_PHYSICS_RIGID_BODY_H
#define APEX_PHYSICS_RIGID_BODY_H

#include "physics_shape.h"
#include "rigid_body_material.h"
#include "../../control.h"

#include <memory>

namespace apex {
class RigidBody : public EntityControl {
public:
    RigidBody(std::shared_ptr<PhysicsShape> shape, double mass);

private:
    std::shared_ptr<PhysicsShape> shape;
    RigidBodyMaterial material;
};
} // namespace apex

#endif