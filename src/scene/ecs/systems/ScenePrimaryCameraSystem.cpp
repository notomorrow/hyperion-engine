/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/ScenePrimaryCameraSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/Scene.hpp>

#include <scene/camera/Camera.hpp>

#include <rendering/RenderScene.hpp>
#include <rendering/RenderCamera.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Camera);

void ScenePrimaryCameraSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    CameraComponent& cameraComponent = GetEntityManager().GetComponent<CameraComponent>(entity);
    if (!cameraComponent.camera.IsValid())
    {
        HYP_LOG(Camera, Error, "CameraComponent added to scene {} entity #{} but camera is invalid", GetEntityManager().GetScene()->GetName(), entity->Id().Value());
        return;
    }

    InitObject(cameraComponent.camera);

    if (!cameraComponent.camera->IsReady())
    {
        HYP_LOG(Camera, Error, "CameraComponent added to scene {} entity #{} but camera is not ready", GetEntityManager().GetScene()->GetName(), entity->Id().Value());
        return;
    }

    EntitySetBase& entitySet = GetEntityManager().GetEntitySet<CameraComponent, EntityTagComponent<EntityTag::CAMERA_PRIMARY>>();

    if (entitySet.Size() > 1)
    {
        HYP_LOG(Camera, Error, "CameraComponent added to scene {} entity #{} but there is already a primary camera", GetEntityManager().GetScene()->GetName(), entity->Id().Value());
        return;
    }
}

void ScenePrimaryCameraSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);
}

void ScenePrimaryCameraSystem::Process(float delta)
{
    // Never called
}

} // namespace hyperion
