/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/ShadowMapUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/camera/Camera.hpp>
#include <scene/camera/OrthoCamera.hpp>

#include <scene/World.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/DirectionalLightShadowRenderer.hpp>
#include <rendering/PointLightShadowRenderer.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderShadowMap.hpp>

#include <core/math/MathUtil.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Shadows);

ShadowMapUpdaterSystem::ShadowMapUpdaterSystem(EntityManager& entityManager)
    : SystemBase(entityManager)
{
}

void ShadowMapUpdaterSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    ShadowMapComponent& shadowMapComponent = GetEntityManager().GetComponent<ShadowMapComponent>(entity);
    // LightComponent& lightComponent = GetEntityManager().GetComponent<LightComponent>(entity);

    // if (shadowMapComponent.subsystem)
    // {
    //     GetWorld()->RemoveSubsystem(shadowMapComponent.subsystem);
    //     shadowMapComponent.subsystem.Reset();
    // }

    // if (!lightComponent.light)
    // {
    //     HYP_LOG(Shadows, Warning, "LightComponent is not valid for Entity #{}", entity->Id().Value());

    //     return;
    // }

    // AddRenderSubsystemToEnvironment(shadowMapComponent, lightComponent);
}

void ShadowMapUpdaterSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    ShadowMapComponent& shadowMapComponent = GetEntityManager().GetComponent<ShadowMapComponent>(entity);
    // LightComponent& lightComponent = GetEntityManager().GetComponent<LightComponent>(entity);

    // if (shadowMapComponent.subsystem)
    // {
    //     GetWorld()->RemoveSubsystem(shadowMapComponent.subsystem);
    //     shadowMapComponent.subsystem.Reset();
    // }
}

void ShadowMapUpdaterSystem::Process(float delta)
{
    // for (auto [entity, shadowMapComponent, lightComponent, transformComponent] : GetEntityManager().GetEntitySet<ShadowMapComponent, LightComponent, TransformComponent>().GetScopedView(GetComponentInfos()))
    // {
    //     if (!lightComponent.light)
    //     {
    //         continue;
    //     }

    //     if (!shadowMapComponent.subsystem.IsValid())
    //     {
    //         continue;
    //     }

    //     // only update shadow map every 10 ticks
    //     if (shadowMapComponent.updateCounter++ % 10 != 0)
    //     {
    //         continue;
    //     }

    //     switch (lightComponent.light->GetLightType())
    //     {
    //     case LT_DIRECTIONAL:
    //     {
    //         DirectionalLightShadowRenderer* shadowRenderer = static_cast<DirectionalLightShadowRenderer*>(shadowMapComponent.subsystem.Get());

    //         const Vec3f& center = transformComponent.transform.GetTranslation();
    //         const Vec3f lightDirection = lightComponent.light->GetPosition().Normalized() * -1.0f;

    //         const Handle<Camera>& shadowCamera = shadowRenderer->GetCamera();

    //         if (!shadowCamera)
    //         {
    //             continue;
    //         }

    //         shadowCamera->SetTranslation(center + lightDirection);
    //         shadowCamera->SetTarget(center);

    //         BoundingBox aabb { center - shadowMapComponent.radius, center + shadowMapComponent.radius };

    //         FixedArray<Vec3f, 8> corners = aabb.GetCorners();

    //         for (Vec3f& corner : corners)
    //         {
    //             corner = shadowCamera->GetViewMatrix() * corner;

    //             aabb.max = MathUtil::Max(aabb.max, corner);
    //             aabb.min = MathUtil::Min(aabb.min, corner);
    //         }

    //         aabb.max.z = shadowMapComponent.radius;
    //         aabb.min.z = -shadowMapComponent.radius;

    //         shadowRenderer->GetCamera()->SetToOrthographicProjection(aabb.min.x, aabb.max.x, aabb.min.y, aabb.max.y, aabb.min.z, aabb.max.z);
    //         shadowRenderer->SetAABB(aabb);

    //         break;
    //     }
    //     case LT_POINT:
    //         // No update needed
    //         break;
    //     default:
    //         break;
    //     }
    // }
}

// void ShadowMapUpdaterSystem::AddRenderSubsystemToEnvironment(ShadowMapComponent& shadowMapComponent, LightComponent& lightComponent)
// {
//     Assert(GetWorld() != nullptr);

//     Assert(lightComponent.light->IsReady());

//     if (shadowMapComponent.subsystem.IsValid())
//     {
//         GetWorld()->RemoveSubsystem(shadowMapComponent.subsystem);
//         shadowMapComponent.subsystem.Reset();
//     }

//     switch (lightComponent.light->GetLightType())
//     {
//     case LT_DIRECTIONAL:
//         shadowMapComponent.subsystem = GetWorld()->AddSubsystem<DirectionalLightShadowRenderer>(
//             GetScene()->HandleFromThis(),
//             lightComponent.light,
//             shadowMapComponent.resolution,
//             shadowMapComponent.mode);

//         break;
//     case LT_POINT:
//         // shadowMapComponent.renderSubsystem = GetWorld()->GetRenderResource().GetEnvironment()->AddRenderSubsystem<PointLightShadowRenderer>(
//         //     GetScene()->HandleFromThis(),
//         //     TResourceHandle<RenderLight>(lightComponent.light->GetRenderResource()),
//         //     shadowMapComponent.resolution);

//         break;
//     default:
//         HYP_LOG(Shadows, Error, "Unsupported light type for shadow map");

//         break;
//     }
// }

} // namespace hyperion
