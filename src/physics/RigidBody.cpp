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

void RigidBody::SetShape(UniquePtr<PhysicsShape> &&shape)
{
    m_shape = std::move(shape);

    if (IsInitCalled()) {
        Engine::Get()->GetWorld()->GetPhysicsWorld().GetAdapter().OnChangePhysicsShape(this);
    }
}

void RigidBody::SetPhysicsMaterial(const PhysicsMaterial &physics_material)
{
    m_physics_material = physics_material;

    if (IsInitCalled()) {
        Engine::Get()->GetWorld()->GetPhysicsWorld().GetAdapter().OnChangePhysicsMaterial(this);
    }
}

void RigidBody::ApplyForce(const Vector3 &force)
{
    Engine::Get()->GetWorld()->GetPhysicsWorld().GetAdapter().ApplyForceToBody(this, force);
}

} // namespace hyperion::v2::physics