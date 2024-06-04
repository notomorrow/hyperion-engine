/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/ShadowMapUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <scene/camera/Camera.hpp>
#include <scene/camera/OrthoCamera.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/DirectionalLightShadowRenderer.hpp>
#include <rendering/PointLightShadowRenderer.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/RenderCollection.hpp>
#include <math/MathUtil.hpp>
#include <core/threading/Threads.hpp>
#include <Engine.hpp>

namespace hyperion {

void ShadowMapUpdaterSystem::OnEntityAdded(ID<Entity> entity)
{
    SystemBase::OnEntityAdded(entity);

    ShadowMapComponent &shadow_map_component = GetEntityManager().GetComponent<ShadowMapComponent>(entity);
    LightComponent &light_component = GetEntityManager().GetComponent<LightComponent>(entity);

    if (!light_component.light) {
        return;
    }

    if (shadow_map_component.render_component) {
        return;
    }

    switch (light_component.light->GetType()) {
    case LightType::DIRECTIONAL:
        shadow_map_component.render_component = GetEntityManager().GetScene()->GetEnvironment()->AddRenderComponent<DirectionalLightShadowRenderer>(
            Name::Unique("shadow_map_renderer_directional"),
            shadow_map_component.resolution,
            shadow_map_component.mode
        );

        break;
    case LightType::POINT:
        shadow_map_component.render_component = GetEntityManager().GetScene()->GetEnvironment()->AddRenderComponent<PointLightShadowRenderer>(
            Name::Unique("shadow_map_renderer_point"),
            light_component.light,
            shadow_map_component.resolution
        );

        break;
    default:
        DebugLog(
            LogType::Warn,
            "Light type %u not supported for shadow mapping\n",
            uint32(light_component.light->GetType())
        );

        break;
    }
}

void ShadowMapUpdaterSystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);

    ShadowMapComponent &shadow_map_component = GetEntityManager().GetComponent<ShadowMapComponent>(entity);
    LightComponent &light_component = GetEntityManager().GetComponent<LightComponent>(entity);

    if (shadow_map_component.render_component) {
        switch (light_component.light->GetType()) {
        case LightType::DIRECTIONAL:
            GetEntityManager().GetScene()->GetEnvironment()->RemoveRenderComponent<DirectionalLightShadowRenderer>(shadow_map_component.render_component->GetName());

            break;
        case LightType::POINT:
            GetEntityManager().GetScene()->GetEnvironment()->RemoveRenderComponent<PointLightShadowRenderer>(shadow_map_component.render_component->GetName());

            break;
        default:
            break;
        }

        shadow_map_component.render_component = nullptr;
    }
}

void ShadowMapUpdaterSystem::Process(GameCounter::TickUnit delta)
{
    for (auto [entity_id, shadow_map_component, light_component, transform_component] : GetEntityManager().GetEntitySet<ShadowMapComponent, LightComponent, TransformComponent>()) {
        if (!light_component.light) {
            continue;
        }

        if (!shadow_map_component.render_component) {
            continue;
        }

        // only update shadow map every 10 ticks
        if (shadow_map_component.update_counter++ % 10 != 0) {
            continue;
        }

        switch (light_component.light->GetType()) {
        case LightType::DIRECTIONAL: {
            DirectionalLightShadowRenderer *shadow_renderer = static_cast<DirectionalLightShadowRenderer *>(shadow_map_component.render_component.Get());

            const Vec3f &center = transform_component.transform.GetTranslation();
            const Vec3f light_direction = light_component.light->GetPosition().Normalized() * -1.0f;

            const Handle<Camera> &shadow_camera = shadow_renderer->GetCamera();

            if (!shadow_camera) {
                continue;
            }

            shadow_camera->SetTranslation(center + light_direction);
            shadow_camera->SetTarget(center);

            BoundingBox aabb { center - shadow_map_component.radius, center + shadow_map_component.radius };

            FixedArray<Vec3f, 8> corners = aabb.GetCorners();

            for (Vec3f &corner : corners) {
                corner = shadow_camera->GetViewMatrix() * corner;

                aabb.max = MathUtil::Max(aabb.max, corner);
                aabb.min = MathUtil::Min(aabb.min, corner);
            }

            aabb.max.z = shadow_map_component.radius;
            aabb.min.z = -shadow_map_component.radius;

            light_component.light->SetShadowMapIndex(shadow_renderer->GetComponentIndex());

            shadow_renderer->GetCamera()->SetToOrthographicProjection(aabb.min.x, aabb.max.x, aabb.min.y, aabb.max.y, aabb.min.z, aabb.max.z);

            shadow_renderer->SetAABB(aabb);

            break;
        }
        case LightType::POINT:
            // No update needed
            break;
        default:
            break;
        }
    }
}

} // namespace hyperion
