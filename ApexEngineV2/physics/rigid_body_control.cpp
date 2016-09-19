#include "rigid_body_control.h"
#include "../entity.h"

namespace apex {
RigidBodyControl::RigidBodyControl(std::shared_ptr<physics::RigidBody> body)
    : EntityControl(60.0), body(body)
{
}

void RigidBodyControl::OnAdded()
{
}

void RigidBodyControl::OnRemoved()
{
}

void RigidBodyControl::OnUpdate(double dt)
{
    parent->SetLocalTranslation(body->GetPosition());
    Quaternion rot = body->GetOrientation();
    rot.Invert();
    parent->SetLocalRotation(rot);
}
} // namespace apex