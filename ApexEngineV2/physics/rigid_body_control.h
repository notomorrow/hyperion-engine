#ifndef RIGID_BODY_CONTROL_H
#define RIGID_BODY_CONTROL_H

#include "../control.h"
#include "rigid_body.h"

#include <memory>

namespace apex {
class RigidBodyControl : public EntityControl {
public:
    RigidBodyControl(std::shared_ptr<RigidBody> body);

    void OnAdded();
    void OnRemoved();
    void OnUpdate(double dt);

private:
    std::shared_ptr<RigidBody> body;
};
} // namespace apex

#endif