#ifndef RIGID_BODY_CONTROL_H
#define RIGID_BODY_CONTROL_H

#include "../control.h"
#include "physics_def.h"
#if EXPERIMENTAL_PHYSICS
#include "physics2/rigid_body2.h"
#else
#include "rigid_body.h"
#endif

#include <memory>

namespace apex {
class RigidBodyControl : public EntityControl {
public:
    RigidBodyControl(std::shared_ptr<RIGID_BODY> body);

    void OnAdded();
    void OnRemoved();
    void OnUpdate(double dt);

private:
    std::shared_ptr<RIGID_BODY> body;
};
} // namespace apex

#endif