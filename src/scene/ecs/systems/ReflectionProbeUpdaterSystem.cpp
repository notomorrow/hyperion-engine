/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/ReflectionProbeUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/EnvProbe.hpp>

#include <rendering/ReflectionProbeRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderEnvProbe.hpp>

#include <core/math/MathUtil.hpp>

#include <core/containers/HashSet.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/Logger.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(EnvProbe);

ReflectionProbeUpdaterSystem::ReflectionProbeUpdaterSystem(EntityManager &entity_manager)
    : System(entity_manager)
{
    m_delegate_handlers.Add(NAME("OnWorldChange"), OnWorldChanged.Bind([this](World *new_world, World *previous_world)
    {
        Threads::AssertOnThread(g_game_thread);

        // Trigger removal and addition of render subsystems

        for (auto [entity_id, reflection_probe_component, transform_component, bounding_box_component] : GetEntityManager().GetEntitySet<ReflectionProbeComponent, TransformComponent, BoundingBoxComponent>().GetScopedView(GetComponentInfos())) {
            if (reflection_probe_component.reflection_probe_renderer) {
                reflection_probe_component.reflection_probe_renderer->RemoveFromEnvironment();

                if (new_world != nullptr) {
                    new_world->GetRenderResource().GetEnvironment()->AddRenderSubsystem(reflection_probe_component.reflection_probe_renderer);
                }
            } else if (!previous_world) {
                OnEntityAdded(Handle<Entity> { entity_id });
            }
        }
    }));
}

void ReflectionProbeUpdaterSystem::OnEntityAdded(const Handle<Entity> &entity)
{
    SystemBase::OnEntityAdded(entity);

    ReflectionProbeComponent &reflection_probe_component = GetEntityManager().GetComponent<ReflectionProbeComponent>(entity);
    BoundingBoxComponent &bounding_box_component = GetEntityManager().GetComponent<BoundingBoxComponent>(entity);
    TransformComponent &transform_component = GetEntityManager().GetComponent<TransformComponent>(entity);

    BoundingBox world_aabb = bounding_box_component.world_aabb;

    if (!world_aabb.IsFinite()) {
        world_aabb = BoundingBox::Empty();
    }

    if (!world_aabb.IsValid()) {
        HYP_LOG(EnvProbe, Warning, "Entity #{} has invalid bounding box", entity.GetID().Value());
    }

    if (reflection_probe_component.env_probe.IsValid()) {
        reflection_probe_component.env_probe->SetParentScene(GetScene()->HandleFromThis());
        reflection_probe_component.env_probe->SetAABB(world_aabb);
    } else {
        reflection_probe_component.env_probe = CreateObject<EnvProbe>(
            GetScene()->HandleFromThis(),
            world_aabb,
            reflection_probe_component.dimensions,
            ENV_PROBE_TYPE_REFLECTION
        );
    }

    reflection_probe_component.env_probe->SetOrigin(transform_component.transform.GetTranslation());

    InitObject(reflection_probe_component.env_probe);

    GetEntityManager().RemoveTag<EntityTag::UPDATE_ENV_PROBE_TRANSFORM>(entity);

    if (!GetWorld()) {
        return;
    }

    if (!reflection_probe_component.reflection_probe_renderer) {
        if (GetScene()->IsForegroundScene()) {
            reflection_probe_component.reflection_probe_renderer = GetWorld()->GetRenderResource().GetEnvironment()->AddRenderSubsystem<ReflectionProbeRenderer>(
                Name::Unique("reflection_probe"),
                TResourceHandle<EnvProbeRenderResource>(reflection_probe_component.env_probe->GetRenderResource())
            );
        }
    }
}

void ReflectionProbeUpdaterSystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);

    ReflectionProbeComponent &reflection_probe_component = GetEntityManager().GetComponent<ReflectionProbeComponent>(entity);

    if (reflection_probe_component.env_probe.IsValid()) {
        reflection_probe_component.env_probe->SetParentScene(Handle<Scene>::empty);
    }

    if (reflection_probe_component.reflection_probe_renderer != nullptr) {
        reflection_probe_component.reflection_probe_renderer->RemoveFromEnvironment();

        reflection_probe_component.reflection_probe_renderer = nullptr;
    }
}

void ReflectionProbeUpdaterSystem::Process(GameCounter::TickUnit delta)
{
    for (auto [entity_id, reflection_probe_component] : GetEntityManager().GetEntitySet<ReflectionProbeComponent>().GetScopedView(GetComponentInfos())) {
        const Handle<EnvProbe> &env_probe = reflection_probe_component.env_probe;

        if (!env_probe.IsValid()) {
            continue;
        }

        env_probe->Update(delta);

        const Handle<Camera> &camera = GetScene()->GetCamera();

        if (!camera.IsValid()) {
            continue;
        }

        const bool is_env_probe_in_frustum = camera->GetFrustum().ContainsAABB(env_probe->GetAABB());

        env_probe->SetIsVisible(camera.GetID(), is_env_probe_in_frustum);
    }

    { // Update transforms and bounding boxes of EnvProbes to match the components
        HashSet<ID<Entity>> updated_entity_ids;

        for (auto [entity_id, reflection_probe_component, transform_component, bounding_box_component, _] : GetEntityManager().GetEntitySet<ReflectionProbeComponent, TransformComponent, BoundingBoxComponent, EntityTagComponent<EntityTag::UPDATE_ENV_PROBE_TRANSFORM>>().GetScopedView(GetComponentInfos())) {
            const Handle<EnvProbe> &env_probe = reflection_probe_component.env_probe;

            if (!env_probe.IsValid()) {
                continue;
            }

            // @FIXME: This is a hack to update the AABB of the reflection probe renderer,
            // EnvProbe should be on the component
            env_probe->SetAABB(bounding_box_component.world_aabb);
            env_probe->SetOrigin(transform_component.transform.GetTranslation());

            updated_entity_ids.Insert(entity_id);
        }

        if (updated_entity_ids.Any()) {
            AfterProcess([this, updated_entity_ids = std::move(updated_entity_ids)]()
            {
                for (const ID<Entity> &entity_id : updated_entity_ids) {
                    GetEntityManager().RemoveTag<EntityTag::UPDATE_ENV_PROBE_TRANSFORM>(entity_id);
                }
            });
        }
    }
}

} // namespace hyperion
