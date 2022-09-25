#include <scene/controllers/physics/RigidBodyController.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

RigidBodyController::RigidBodyController(
    UniquePtr<physics::PhysicsShape> &&shape,
    const physics::PhysicsMaterial &physics_material
) : Controller("RigidBodyController"),
    m_shape(std::move(shape)),
    m_physics_material(physics_material)
{
}

void RigidBodyController::OnAdded()
{
    m_rigid_body = GetEngine()->CreateHandle<physics::RigidBody>(
        std::move(m_shape),
        m_physics_material
    );

    GetEngine()->InitObject(m_rigid_body);

    m_rigid_body->SetTransform(GetOwner()->GetTransform());    
}

void RigidBodyController::OnRemoved() { }

void RigidBodyController::OnAttachedToScene(Scene *scene)
{
    GetEngine()->GetWorld().GetPhysicsWorld().AddRigidBody(Handle<physics::RigidBody>(m_rigid_body));
}

void RigidBodyController::OnDetachedFromScene(Scene *scene)
{
    GetEngine()->GetWorld().GetPhysicsWorld().RemoveRigidBody(m_rigid_body);
}

void RigidBodyController::OnUpdate(GameCounter::TickUnit delta)
{
    GetOwner()->SetTransform(m_rigid_body->GetTransform());
}

} // namespace hyperion::v2