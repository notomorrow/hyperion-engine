#ifndef PHYSICS_CONTROL_H
#define PHYSICS_CONTROL_H

#include "../control.h"
#include "physics_object.h"
#include "../math/vector3.h"

namespace apex {
class PhysicsControl : public EntityControl {
public:
    PhysicsControl(PhysicsObject *shape);
    virtual ~PhysicsControl();

    virtual void OnAdded();
    virtual void OnRemoved();
    virtual void OnUpdate(double dt);

    const PhysicsObject *GetPhysicsObject() const;

protected:
    PhysicsObject *object;
};
}

#endif