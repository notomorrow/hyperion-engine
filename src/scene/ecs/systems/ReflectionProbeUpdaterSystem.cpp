/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/ReflectionProbeUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <rendering/EnvProbe.hpp>
#include <rendering/ReflectionProbeRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>

#include <math/MathUtil.hpp>

#include <core/threading/Threads.hpp>
#include <core/logging/Logger.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(EnvProbe);

void ReflectionProbeUpdaterSystem::OnEntityAdded(const Handle<Entity> &entity)
{
    SystemBase::OnEntityAdded(entity);

    ReflectionProbeComponent &reflection_probe_component = GetEntityManager().GetComponent<ReflectionProbeComponent>(entity);
    BoundingBoxComponent &bounding_box_component = GetEntityManager().GetComponent<BoundingBoxComponent>(entity);

    if (reflection_probe_component.reflection_probe_renderer != nullptr) {
        return;
    }

    BoundingBox world_aabb = bounding_box_component.world_aabb;

    if (!world_aabb.IsFinite()) {
        world_aabb = BoundingBox::Empty();
    }

    if (!world_aabb.IsValid()) {
        HYP_LOG(EnvProbe, LogLevel::WARNING, "Entity #{} has invalid bounding box", entity.GetID().Value());
    }

    if (!(GetEntityManager().GetScene()->GetFlags() & (SceneFlags::NON_WORLD | SceneFlags::DETACHED))) {
        reflection_probe_component.reflection_probe_renderer = GetEntityManager().GetScene()->GetEnvironment()->AddRenderSubsystem<ReflectionProbeRenderer>(
            Name::Unique("reflection_probe"),
            world_aabb
        );
    }
}

void ReflectionProbeUpdaterSystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);

    ReflectionProbeComponent &reflection_probe_component = GetEntityManager().GetComponent<ReflectionProbeComponent>(entity);

    if (reflection_probe_component.reflection_probe_renderer != nullptr) {
        GetEntityManager().GetScene()->GetEnvironment()->RemoveRenderSubsystem<ReflectionProbeRenderer>(reflection_probe_component.reflection_probe_renderer->GetName());

        reflection_probe_component.reflection_probe_renderer = nullptr;
    }
}

void ReflectionProbeUpdaterSystem::Process(GameCounter::TickUnit delta)
{
    for (auto [entity_id, reflection_probe_component, transform_component, bounding_box_component] : GetEntityManager().GetEntitySet<ReflectionProbeComponent, TransformComponent, BoundingBoxComponent>().GetScopedView(GetComponentInfos())) {
        if (!reflection_probe_component.reflection_probe_renderer) {
            continue;
        }
        
        const HashCode transform_hash_code = transform_component.transform.GetHashCode();

        const BoundingBox &world_aabb = bounding_box_component.world_aabb;

        if (reflection_probe_component.transform_hash_code != transform_hash_code || reflection_probe_component.reflection_probe_renderer->GetAABB() != world_aabb) {
            // @TODO
            // reflection_probe_component.reflection_probe_renderer->SetAABB(world_aabb);

            reflection_probe_component.transform_hash_code = transform_hash_code;
        }
    }
}

} // namespace hyperion
