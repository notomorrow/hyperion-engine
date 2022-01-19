#ifndef RIGID_BODY_CONTROL_H
#define RIGID_BODY_CONTROL_H

#include "../control.h"

namespace apex {
namespace physics {
class RigidBodyControl : public EntityControl {
public:
    RigidBodyControl();
    ~RigidBodyControl();

    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(double dt) override;
};
}
} // namespace apex

#endif
