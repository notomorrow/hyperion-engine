/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/PhysicsSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <Engine.hpp>

namespace hyperion {

void PhysicsSystem::OnEntityAdded(ID<Entity> entity)
{
    SystemBase::OnEntityAdded(entity);

    if (!GetEntityManager().GetScene()->GetWorld()) {
        return;
    }

    RigidBodyComponent &rigid_body_component = GetEntityManager().GetComponent<RigidBodyComponent>(entity);
    TransformComponent &transform_component = GetEntityManager().GetComponent<TransformComponent>(entity);

    if (rigid_body_component.rigid_body) {
        InitObject(rigid_body_component.rigid_body);

        rigid_body_component.rigid_body->SetTransform(transform_component.transform);
        rigid_body_component.transform_hash_code = transform_component.transform.GetHashCode();

        rigid_body_component.flags |= RIGID_BODY_COMPONENT_FLAG_INIT;

        GetEntityManager().GetScene()->GetWorld()->GetPhysicsWorld().AddRigidBody(rigid_body_component.rigid_body);
    }
}

void PhysicsSystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);

    if (!GetEntityManager().GetScene()->GetWorld()) {
        return;
    }

    RigidBodyComponent &rigid_body_component = GetEntityManager().GetComponent<RigidBodyComponent>(entity);

    if (rigid_body_component.rigid_body) {
        GetEntityManager().GetScene()->GetWorld()->GetPhysicsWorld().RemoveRigidBody(rigid_body_component.rigid_body);
    }
}

void PhysicsSystem::Process(GameCounter::TickUnit delta)
{
    for (auto [entity_id, rigid_body_component, transform_component] : GetEntityManager().GetEntitySet<RigidBodyComponent, TransformComponent>().GetScopedView(GetComponentInfos())) {
        Handle<physics::RigidBody> &rigid_body = rigid_body_component.rigid_body;
        Transform &transform = transform_component.transform;

        if (!rigid_body) {
            continue;
        }

        Transform &rigid_body_transform = rigid_body->GetTransform();

        transform.SetTranslation(rigid_body_transform.GetTranslation());
        transform.SetRotation(rigid_body_transform.GetRotation());
    }
}

} // namespace hyperion
