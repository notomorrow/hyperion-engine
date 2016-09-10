#include "gravity_force.h"

namespace apex {
const Vector3 GravityForce::GRAVITY = Vector3(0, -9.81, 0);

void GravityForce::UpdateOnBody(RigidBody *body, double dt)
{
    // do not apply to bodies with infinite mass
    if (!body->HasFiniteMass()) {
        return;
    }
    body->ApplyForce(GRAVITY * body->GetMass());
}
} // namespace apex