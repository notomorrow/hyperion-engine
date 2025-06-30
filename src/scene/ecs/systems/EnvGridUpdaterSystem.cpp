/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/EnvGridUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/NodeLinkComponent.hpp>

#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/Node.hpp>
#include <scene/View.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderSubsystem.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderEnvGrid.hpp>

#include <rendering/backend/RendererShader.hpp>

#include <core/math/MathUtil.hpp>

#include <core/threading/Threads.hpp>

#include <core/object/HypClassRegistry.hpp>
#include <core/object/HypEnum.hpp>

#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <system/AppContext.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(EnvGrid);

#pragma region EnvGridUpdaterSystem

EnvGridUpdaterSystem::EnvGridUpdaterSystem(EntityManager& entityManager)
    : SystemBase(entityManager)
{
}

void EnvGridUpdaterSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    // EnvGridComponent& envGridComponent = GetEntityManager().GetComponent<EnvGridComponent>(entity);
    // BoundingBoxComponent& boundingBoxComponent = GetEntityManager().GetComponent<BoundingBoxComponent>(entity);

    // if (envGridComponent.envGrid)
    // {
    //     return;
    // }

    // if (!GetWorld())
    // {
    //     return;
    // }

    // EnumFlags<EnvGridFlags> flags = EnvGridFlags::NONE;

    // if (g_engine->GetAppContext()->GetConfiguration().Get("rendering.env_grid.reflections.enabled").ToBool()
    //     || g_engine->GetAppContext()->GetConfiguration().Get("rendering.env_grid.gi.mode").ToString().ToLower() == "voxel")
    // {
    //     flags |= EnvGridFlags::USE_VOXEL_GRID;
    // }

    // envGridComponent.envGrid = CreateObject<EnvGrid>(
    //     GetScene()->HandleFromThis(),
    //     boundingBoxComponent.worldAabb,
    //     EnvGridOptions {
    //         .type = envGridComponent.envGridType,
    //         .density = envGridComponent.gridSize,
    //         .flags = flags });

    // InitObject(envGridComponent.envGrid);
}

void EnvGridUpdaterSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    // EnvGridComponent& envGridComponent = GetEntityManager().GetComponent<EnvGridComponent>(entity);

    // envGridComponent.envGrid.Reset();

    // if (envGridComponent.envGridRenderSubsystem)
    // {
    //     envGridComponent.envGridRenderSubsystem->RemoveFromEnvironment();
    //     envGridComponent.envGridRenderSubsystem.Reset();
    // }
}

void EnvGridUpdaterSystem::Process(float delta)
{
    // { // Determine which EnvGrids needs to be updated on the game thread
    //     AfterProcess([this]()
    //         {
    //             for (auto [entity, _, boundingBoxComponent] : GetEntityManager().GetEntitySet<EntityType<EnvGrid>, BoundingBoxComponent>().GetScopedView(GetComponentInfos()))
    //             {
    //                 // @TODO Store pointer to Entity rather than Id so we don't need to lock the handle
    //                 Handle<Entity> entity { entityId };
    //                 AssertDebug(entity->IsA<EnvGrid>());

    //                 Handle<EnvGrid> envGrid = entity.Cast<EnvGrid>();

    //                 envGrid->SetReceivesUpdate(true);
    //             }
    //         });
    // }

    // { // Update camera data
    //     HashSet<ObjId<Entity>> updatedEntityIds;

    //     for (auto [entity, _t, transformComponent, boundingBoxComponent, _] : GetEntityManager().GetEntitySet<EntityType<EnvGrid>, TransformComponent, BoundingBoxComponent, EntityTagComponent<EntityTag::UPDATE_ENV_GRID_TRANSFORM>>().GetScopedView(GetComponentInfos()))
    //     {
    //         if (!envGridComponent.envGrid)
    //         {
    //             continue;
    //         }

    //         envGridComponent.envGrid->Translate(boundingBoxComponent.worldAabb, transformComponent.transform.GetTranslation());

    //         updatedEntityIds.Insert(entityId);
    //     }

    //     if (updatedEntityIds.Any())
    //     {
    //         AfterProcess([this, updatedEntityIds = std::move(updatedEntityIds)]()
    //             {
    //                 for (const ObjId<Entity>& entityId : updatedEntityIds)
    //                 {
    //                     GetEntityManager().RemoveTag<EntityTag::UPDATE_ENV_GRID_TRANSFORM>(entityId);
    //                 }
    //             });
    //     }
    // }

    // { // Update EnvGrid transforms to map to the camera
    //     HashSet<ObjId<Entity>> updatedEntityIds;

    //     for (auto [entity, envGridComponent, transformComponent, boundingBoxComponent] : GetEntityManager().GetEntitySet<EnvGridComponent, TransformComponent, BoundingBoxComponent>().GetScopedView(GetComponentInfos()))
    //     {
    //         if (!envGridComponent.envGrid)
    //         {
    //             continue;
    //         }

    //         // Update movable envgrids
    //         if (envGridComponent.mobility & EnvGridMobility::FOLLOW_CAMERA)
    //         {
    //             const Handle<Camera>& camera = GetScene()->GetPrimaryCamera();

    //             if (camera.IsValid())
    //             {
    //                 Vec3f translation = transformComponent.transform.GetTranslation();

    //                 if (envGridComponent.mobility & EnvGridMobility::FOLLOW_CAMERA_X)
    //                 {
    //                     translation.x = camera->GetTranslation().x;
    //                 }

    //                 if (envGridComponent.mobility & EnvGridMobility::FOLLOW_CAMERA_Y)
    //                 {
    //                     translation.y = camera->GetTranslation().y;
    //                 }

    //                 if (envGridComponent.mobility & EnvGridMobility::FOLLOW_CAMERA_Z)
    //                 {
    //                     translation.z = camera->GetTranslation().z;
    //                 }

    //                 if (translation != transformComponent.transform.GetTranslation())
    //                 {
    //                     transformComponent.transform.SetTranslation(translation);

    //                     updatedEntityIds.Insert(entityId);
    //                 }
    //             }
    //         }
    //     }

    //     if (updatedEntityIds.Any())
    //     {
    //         AfterProcess([this, updatedEntityIds = std::move(updatedEntityIds)]()
    //             {
    //                 for (const ObjId<Entity>& entityId : updatedEntityIds)
    //                 {
    //                     GetEntityManager().AddTag<EntityTag::UPDATE_ENV_GRID_TRANSFORM>(entityId);
    //                 }
    //             });
    //     }
    // }
}

#pragma endregion EnvGridUpdaterSystem

} // namespace hyperion
