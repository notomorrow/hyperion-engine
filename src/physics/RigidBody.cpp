#include <physics/RigidBody.hpp>

#include <Engine.hpp>

namespace hyperion::v2::physics {

RigidBody::~RigidBody()
{
}

void RigidBody::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    // do nothing
}

void RigidBody::ApplyForce(const Vector3 &force)
{
    Engine::Get()->GetWorld()->GetPhysicsWorld().GetAdapter().ApplyForceToBody(this, force);
}

} // namespace hyperion::v2::physics