/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/CameraSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/Scene.hpp>
#include <scene/World.hpp>

#include <scene/world_grid/WorldGrid.hpp>

#include <scene/camera/Camera.hpp>
#include <scene/camera/streaming/CameraStreamingVolume.hpp>

#include <streaming/StreamingManager.hpp>

#include <core/containers/HashSet.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Camera);

void CameraSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    HYP_LOG(Camera, Debug, "CameraSystem::OnEntityAdded: CameraComponent added to scene {} entity #{}", GetEntityManager().GetScene()->GetName(), entity->Id().Value());

    CameraComponent& cameraComponent = GetEntityManager().GetComponent<CameraComponent>(entity);
    InitObject(cameraComponent.camera);

    if (World* world = GetWorld())
    {
        if (cameraComponent.camera.IsValid() && cameraComponent.camera->GetStreamingVolume().IsValid())
        {
            world->GetWorldGrid()->GetStreamingManager()->AddStreamingVolume(cameraComponent.camera->GetStreamingVolume());
        }
    }
}

void CameraSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    CameraComponent& cameraComponent = GetEntityManager().GetComponent<CameraComponent>(entity);

    if (World* world = GetWorld())
    {
        if (cameraComponent.camera.IsValid() && cameraComponent.camera->GetStreamingVolume().IsValid())
        {
            world->GetWorldGrid()->GetStreamingManager()->RemoveStreamingVolume(cameraComponent.camera->GetStreamingVolume());
        }
    }

    HYP_LOG(Camera, Debug, "CameraSystem::OnEntityRemoved: CameraComponent removed from scene {} entity #{}", GetEntityManager().GetScene()->GetName(), entity->Id());
}

void CameraSystem::Process(float delta)
{
    HashSet<WeakHandle<Entity>> updatedEntities;

    Scene* scene = GetEntityManager().GetScene();

    for (auto [entity, cameraComponent, transformComponent, _] : GetEntityManager().GetEntitySet<CameraComponent, TransformComponent, EntityTagComponent<EntityTag::UPDATE_CAMERA_TRANSFORM>>().GetScopedView(GetComponentInfos()))
    {
        if (!cameraComponent.camera.IsValid())
        {
            continue;
        }

        cameraComponent.camera->SetTranslation(transformComponent.transform.GetTranslation());
        cameraComponent.camera->SetDirection((transformComponent.transform.GetRotation() * Vec3f(0.0f, 0.0f, 1.0f)).Normalize());
    }

    for (auto [entity, cameraComponent] : GetEntityManager().GetEntitySet<CameraComponent>().GetScopedView(GetComponentInfos()))
    {
        if (!cameraComponent.camera.IsValid())
        {
            continue;
        }

        cameraComponent.camera->Update(delta);

        updatedEntities.Insert(entity->WeakHandleFromThis());
    }

    for (auto [entity, cameraComponent, nodeLinkComponent] : GetEntityManager().GetEntitySet<CameraComponent, NodeLinkComponent>().GetScopedView(GetComponentInfos()))
    {
        if (!cameraComponent.camera.IsValid())
        {
            continue;
        }

        if (!nodeLinkComponent.node.IsValid())
        {
            continue;
        }

        Handle<Node> node = nodeLinkComponent.node.Lock();

        if (!node)
        {
            continue;
        }

        Transform newTransform(cameraComponent.camera->GetTranslation(), Vec3f::One(), cameraComponent.camera->GetViewMatrix().ExtractRotation());
        node->SetWorldTransform(newTransform);
    }

    if (updatedEntities.Any())
    {
        AfterProcess([this, updatedEntities = std::move(updatedEntities)]()
            {
                for (const WeakHandle<Entity>& entityWeak : updatedEntities)
                {
                    GetEntityManager().RemoveTag<EntityTag::UPDATE_CAMERA_TRANSFORM>(entityWeak.GetUnsafe());
                }
            });
    }
}

} // namespace hyperion
