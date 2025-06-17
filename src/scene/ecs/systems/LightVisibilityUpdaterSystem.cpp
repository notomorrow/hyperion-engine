/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#if 0
#include <scene/ecs/systems/LightVisibilityUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/Scene.hpp>
#include <scene/Light.hpp>

#include <core/containers/HashSet.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion {

void LightVisibilityUpdaterSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    GetEntityManager().AddTag<EntityTag::LIGHT>(entity);

    LightComponent& lightComponent = GetEntityManager().GetComponent<LightComponent>(entity);

    if (!lightComponent.light)
    {
        return;
    }

    Handle<Light> light = lightComponent.light;
    InitObject(light);

    const Transform initialTransform(light->GetPosition());

    // Set initial transform on the TransformComponent
    TransformComponent* transformComponent = GetEntityManager().TryGetComponent<TransformComponent>(entity);

    if (!transformComponent)
    {
        GetEntityManager().AddComponent<TransformComponent>(entity, TransformComponent { initialTransform });
        transformComponent = &GetEntityManager().GetComponent<TransformComponent>(entity);
    }

    transformComponent->transform = initialTransform;

    const Transform transform = transformComponent->transform;

    { // Add a bounding box component to the entity or update if it already exists
        BoundingBoxComponent* boundingBoxComponent = GetEntityManager().TryGetComponent<BoundingBoxComponent>(entity);

        if (!boundingBoxComponent)
        {
            GetEntityManager().AddComponent<BoundingBoxComponent>(entity, BoundingBoxComponent {});
            boundingBoxComponent = &GetEntityManager().GetComponent<BoundingBoxComponent>(entity);
        }

        switch (light->GetLightType())
        {
        case LT_DIRECTIONAL:
            boundingBoxComponent->localAabb = BoundingBox::Infinity();
            boundingBoxComponent->worldAabb = BoundingBox::Infinity();
            break;
        case LT_POINT: // fallthrough
        case LT_AREA_RECT:
            boundingBoxComponent->localAabb = light->GetAABB();
            boundingBoxComponent->worldAabb = transform * boundingBoxComponent->localAabb;
            break;
        default:
            break;
        }
    }

    { // Add a VisibilityStateComponent if it doesn't exist yet
        VisibilityStateComponent* visibilityStateComponent = GetEntityManager().TryGetComponent<VisibilityStateComponent>(entity);

        if (!visibilityStateComponent)
        {
            // Directional light sources are always visible
            if (light->GetLightType() == LT_DIRECTIONAL)
            {
                GetEntityManager().AddComponent<VisibilityStateComponent>(entity, VisibilityStateComponent { VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE });
            }
            else
            {
                GetEntityManager().AddComponent<VisibilityStateComponent>(entity, VisibilityStateComponent {});
            }
        }
    }

    GetEntityManager().RemoveTag<EntityTag::UPDATE_LIGHT_TRANSFORM>(entity);
}

void LightVisibilityUpdaterSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    GetEntityManager().RemoveTag<EntityTag::LIGHT>(entity);
}

void LightVisibilityUpdaterSystem::Process(float delta)
{
    { // Invalidate visibility state of directional lights that have had their transforms update to refresh the octree
        for (auto [entity, lightComponent, visibilityStateComponent, _] : GetEntityManager().GetEntitySet<LightComponent, VisibilityStateComponent, EntityTagComponent<EntityTag::UPDATE_LIGHT_TRANSFORM>>().GetScopedView(GetComponentInfos()))
        {
            if (!lightComponent.light.IsValid() || !lightComponent.light->IsReady())
            {
                continue;
            }

            if (lightComponent.light->GetLightType() == LT_DIRECTIONAL)
            {
                visibilityStateComponent.flags |= VISIBILITY_STATE_FLAG_INVALIDATED;
            }
        }
    }

    { // Update light transforms if they have the UPDATE_LIGHT_TRANSFORM tag
        HashSet<WeakHandle<Entity>> updatedEntities;

        for (auto [entity, lightComponent, transformComponent, _] : GetEntityManager().GetEntitySet<LightComponent, TransformComponent, EntityTagComponent<EntityTag::UPDATE_LIGHT_TRANSFORM>>().GetScopedView(GetComponentInfos()))
        {
            if (!lightComponent.light.IsValid() || !lightComponent.light->IsReady())
            {
                continue;
            }

            if (lightComponent.light->GetLightType() == LT_DIRECTIONAL)
            {
                // Normalize the translation*rotation to set the direction of directional lights
                lightComponent.light->SetPosition((transformComponent.transform.GetTranslation() * transformComponent.transform.GetRotation()).Normalized());
            }
            else
            {
                lightComponent.light->SetPosition(transformComponent.transform.GetTranslation());
            }

            updatedEntities.Insert(entity->WeakHandleFromThis());
        }

        if (updatedEntities.Any())
        {
            AfterProcess([this, updatedEntities = std::move(updatedEntities)]()
                {
                    for (const WeakHandle<Entity>& entityWeak : updatedEntities)
                    {
                        GetEntityManager().RemoveTag<EntityTag::UPDATE_LIGHT_TRANSFORM>(entityWeak.GetUnsafe());
                    }
                });
        }
    }

    // Recalculate light visibility for the scene's camera
    for (auto [entity, lightComponent, transformComponent, boundingBoxComponent] : GetEntityManager().GetEntitySet<LightComponent, TransformComponent, BoundingBoxComponent>().GetScopedView(GetComponentInfos()))
    {
        if (!lightComponent.light.IsValid() || !lightComponent.light->IsReady())
        {
            continue;
        }

        for (auto [cameraEntityId, cameraComponent] : GetEntityManager().GetEntitySet<CameraComponent>().GetScopedView(GetComponentInfos()))
        {
            const Handle<Camera>& camera = cameraComponent.camera;

            if (!camera.IsValid())
            {
                continue;
            }

            // For area lights, update the material Id if the entity has a MeshComponent.
            if (lightComponent.light->GetLightType() == LT_AREA_RECT)
            {
                /*if (MeshComponent *meshComponent = entityManager.TryGetComponent<MeshComponent>(entity)) {
                    lightComponent.light->SetMaterial(meshComponent->material);
                } else {
                    lightComponent.light->SetMaterial(Handle<Material>::empty);
                }*/
            }
        }

        if (lightComponent.light->GetMutationState().IsDirty())
        {
            lightComponent.light->EnqueueRenderUpdates();
        }
    }
}

} // namespace hyperion

#endif