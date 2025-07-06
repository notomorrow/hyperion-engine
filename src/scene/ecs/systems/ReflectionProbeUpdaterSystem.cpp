/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/ReflectionProbeUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/EnvProbe.hpp>

#include <rendering/ReflectionProbeRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderEnvProbe.hpp>

#include <core/math/MathUtil.hpp>

#include <core/containers/HashSet.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/Logger.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(EnvProbe);

ReflectionProbeUpdaterSystem::ReflectionProbeUpdaterSystem(EntityManager& entityManager)
    : SystemBase(entityManager)
{
}

void ReflectionProbeUpdaterSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    // ReflectionProbeComponent& reflectionProbeComponent = GetEntityManager().GetComponent<ReflectionProbeComponent>(entity);
    // BoundingBoxComponent& boundingBoxComponent = GetEntityManager().GetComponent<BoundingBoxComponent>(entity);
    // TransformComponent& transformComponent = GetEntityManager().GetComponent<TransformComponent>(entity);

    // BoundingBox worldAabb = boundingBoxComponent.worldAabb;

    // if (!worldAabb.IsFinite())
    // {
    //     worldAabb = BoundingBox::Empty();
    // }

    // if (!worldAabb.IsValid())
    // {
    //     HYP_LOG(EnvProbe, Warning, "Entity #{} has invalid bounding box", entity.Id().Value());
    // }

    // if (reflectionProbeComponent.reflectionProbeRenderer)
    // {
    //     reflectionProbeComponent.reflectionProbeRenderer->RemoveFromEnvironment();
    //     reflectionProbeComponent.reflectionProbeRenderer.Reset();
    // }

    // if (reflectionProbeComponent.envProbe.IsValid())
    // {
    //     reflectionProbeComponent.envProbe->SetParentScene(GetScene()->HandleFromThis());
    //     reflectionProbeComponent.envProbe->SetAABB(worldAabb);
    // }
    // else
    // {
    //     reflectionProbeComponent.envProbe = CreateObject<EnvProbe>(
    //         GetScene()->HandleFromThis(),
    //         worldAabb,
    //         reflectionProbeComponent.dimensions,
    //         EPT_REFLECTION);
    // }

    // reflectionProbeComponent.envProbe->SetOrigin(transformComponent.transform.GetTranslation());

    // InitObject(reflectionProbeComponent.envProbe);

    // GetEntityManager().RemoveTag<EntityTag::UPDATE_ENV_PROBE_TRANSFORM>(entity);

    // AddRenderSubsystemToEnvironment(reflectionProbeComponent);
}

void ReflectionProbeUpdaterSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    // ReflectionProbeComponent& reflectionProbeComponent = GetEntityManager().GetComponent<ReflectionProbeComponent>(entity);

    // if (reflectionProbeComponent.envProbe.IsValid())
    // {
    //     reflectionProbeComponent.envProbe->SetParentScene(Handle<Scene>::empty);
    // }

    // if (reflectionProbeComponent.reflectionProbeRenderer)
    // {
    //     reflectionProbeComponent.reflectionProbeRenderer->RemoveFromEnvironment();
    //     reflectionProbeComponent.reflectionProbeRenderer.Reset();
    // }
}

void ReflectionProbeUpdaterSystem::Process(float delta)
{
    // for (auto [entity, reflectionProbeComponent] : GetEntityManager().GetEntitySet<ReflectionProbeComponent>().GetScopedView(GetComponentInfos()))
    // {
    //     const Handle<EnvProbe>& envProbe = reflectionProbeComponent.envProbe;

    //     if (!envProbe.IsValid())
    //     {
    //         continue;
    //     }

    //     envProbe->Update(delta);

    //     const Handle<Camera>& camera = GetScene()->GetPrimaryCamera();

    //     if (!camera.IsValid())
    //     {
    //         continue;
    //     }

    //     const bool isEnvProbeInFrustum = camera->GetFrustum().ContainsAABB(envProbe->GetAABB());

    //     envProbe->SetIsVisible(camera.Id(), isEnvProbeInFrustum);
    // }

    // { // Update transforms and bounding boxes of EnvProbes to match the components
    //     HashSet<ObjId<Entity>> updatedEntityIds;

    //     for (auto [entity, reflectionProbeComponent, transformComponent, boundingBoxComponent, _] : GetEntityManager().GetEntitySet<ReflectionProbeComponent, TransformComponent, BoundingBoxComponent, EntityTagComponent<EntityTag::UPDATE_ENV_PROBE_TRANSFORM>>().GetScopedView(GetComponentInfos()))
    //     {
    //         const Handle<EnvProbe>& envProbe = reflectionProbeComponent.envProbe;

    //         if (!envProbe.IsValid())
    //         {
    //             continue;
    //         }

    //         // @FIXME: This is a hack to update the AABB of the reflection probe renderer,
    //         // EnvProbe should be on the component
    //         envProbe->SetAABB(boundingBoxComponent.worldAabb);
    //         envProbe->SetOrigin(transformComponent.transform.GetTranslation());

    //         updatedEntityIds.Insert(entityId);
    //     }

    //     if (updatedEntityIds.Any())
    //     {
    //         AfterProcess([this, updatedEntityIds = std::move(updatedEntityIds)]()
    //             {
    //                 for (const ObjId<Entity>& entityId : updatedEntityIds)
    //                 {
    //                     GetEntityManager().RemoveTag<EntityTag::UPDATE_ENV_PROBE_TRANSFORM>(entityId);
    //                 }
    //             });
    //     }
    // }
}

void ReflectionProbeUpdaterSystem::AddRenderSubsystemToEnvironment(ReflectionProbeComponent& reflectionProbeComponent)
{
    if (!GetWorld())
    {
        return;
    }

    // if (reflectionProbeComponent.reflectionProbeRenderer)
    // {
    //     GetWorld()->GetRenderResource().GetEnvironment()->AddRenderSubsystem(reflectionProbeComponent.reflectionProbeRenderer);
    // }
    // else
    // {
    //     if (!reflectionProbeComponent.envProbe.IsValid())
    //     {
    //         HYP_LOG(EnvProbe, Warning, "ReflectionProbeComponent has invalid EnvProbe");

    //         return;
    //     }

    //     InitObject(reflectionProbeComponent.envProbe);

    //     reflectionProbeComponent.reflectionProbeRenderer = GetWorld()->GetRenderResource().GetEnvironment()->AddRenderSubsystem<ReflectionProbeRenderer>(
    //         Name::Unique("ReflectionProbeRenderer"),
    //         TResourceHandle<RenderEnvProbe>(reflectionProbeComponent.envProbe->GetRenderResource()));
    // }
}

} // namespace hyperion
