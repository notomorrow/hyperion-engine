/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/BLASUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/Scene.hpp>
#include <scene/Mesh.hpp>
#include <scene/Material.hpp>
#include <scene/World.hpp>

#include <rendering/RenderMesh.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderEnvironment.hpp>

#include <rendering/backend/RenderCommand.hpp>
#include <rendering/backend/rt/RendererAccelerationStructure.hpp>

#include <core/containers/HashSet.hpp>

#include <core/logging/Logger.hpp>

#include <system/AppContext.hpp>

#include <EngineGlobals.hpp>
#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

#pragma region Render commands

struct RENDER_COMMAND(UpdateBLASTransform)
    : RenderCommand
{
    FixedArray<BLASRef, maxFramesInFlight> bottomLevelAccelerationStructures;
    Matrix4 transform;

    RENDER_COMMAND(UpdateBLASTransform)(
        const FixedArray<BLASRef, maxFramesInFlight>& bottomLevelAccelerationStructures,
        const Matrix4& transform)
        : bottomLevelAccelerationStructures(bottomLevelAccelerationStructures),
          transform(transform)
    {
    }

    virtual ~RENDER_COMMAND(UpdateBLASTransform)() override = default;

    virtual RendererResult operator()() override
    {
        for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
        {
            if (!bottomLevelAccelerationStructures[frameIndex].IsValid())
            {
                continue;
            }

            bottomLevelAccelerationStructures[frameIndex]->SetTransform(transform);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(AddBLASToTLAS)
    : RenderCommand
{
    TResourceHandle<RenderWorld> renderWorld;
    FixedArray<BLASRef, maxFramesInFlight> bottomLevelAccelerationStructures;

    RENDER_COMMAND(AddBLASToTLAS)(
        const TResourceHandle<RenderWorld>& renderWorld,
        const FixedArray<BLASRef, maxFramesInFlight>& bottomLevelAccelerationStructures)
        : renderWorld(renderWorld),
          bottomLevelAccelerationStructures(bottomLevelAccelerationStructures)
    {
    }

    virtual ~RENDER_COMMAND(AddBLASToTLAS)() override = default;

    virtual RendererResult operator()() override
    {
        RenderEnvironment* environment = renderWorld->GetEnvironment();
        Assert(environment != nullptr);

        for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
        {
            if (bottomLevelAccelerationStructures[frameIndex].IsValid())
            {
                environment->GetTopLevelAccelerationStructures()[frameIndex]->AddBLAS(bottomLevelAccelerationStructures[frameIndex]);
            }
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(RemoveBLASFromTLAS)
    : RenderCommand
{
    TResourceHandle<RenderWorld> renderWorld;
    FixedArray<BLASRef, maxFramesInFlight> bottomLevelAccelerationStructures;

    RENDER_COMMAND(RemoveBLASFromTLAS)(
        const TResourceHandle<RenderWorld>& renderWorld,
        const FixedArray<BLASRef, maxFramesInFlight>& bottomLevelAccelerationStructures)
        : renderWorld(renderWorld),
          bottomLevelAccelerationStructures(bottomLevelAccelerationStructures)
    {
    }

    virtual ~RENDER_COMMAND(RemoveBLASFromTLAS)() override = default;

    virtual RendererResult operator()() override
    {
        RenderEnvironment* environment = renderWorld->GetEnvironment();
        Assert(environment != nullptr);

        for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
        {
            if (bottomLevelAccelerationStructures[frameIndex].IsValid())
            {
                environment->GetTopLevelAccelerationStructures()[frameIndex]->RemoveBLAS(bottomLevelAccelerationStructures[frameIndex]);
            }
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

BLASUpdaterSystem::BLASUpdaterSystem(EntityManager& entityManager)
    : SystemBase(entityManager)
{
}

bool BLASUpdaterSystem::ShouldCreateForScene(Scene* scene) const
{
    if (!scene->IsForegroundScene())
    {
        return false;
    }

    if (scene->GetFlags() & SceneFlags::UI)
    {
        return false;
    }

    return true;
}

void BLASUpdaterSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    if (!g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.enabled").ToBool())
    {
        return;
    }

    MeshComponent& meshComponent = GetEntityManager().GetComponent<MeshComponent>(entity);
    TransformComponent& transformComponent = GetEntityManager().GetComponent<TransformComponent>(entity);

    if (!meshComponent.mesh.IsValid() || !meshComponent.material.IsValid())
    {
        return;
    }

    Assert(meshComponent.raytracingData == nullptr);

    InitObject(meshComponent.mesh);
    Assert(meshComponent.mesh->IsReady());

    InitObject(meshComponent.material);
    Assert(meshComponent.material->IsReady());

    BLASRef blas;

    {
        TResourceHandle<RenderMesh> meshResourceHandle(meshComponent.mesh->GetRenderResource());

        blas = meshResourceHandle->BuildBLAS(meshComponent.material);

        if (!blas)
        {
            HYP_LOG(Rendering, Error, "Failed to build BLAS for mesh #{} ({})", meshComponent.mesh.Id().Value(), meshComponent.mesh->GetName());

            return;
        }
    }

    blas->SetTransform(transformComponent.transform.GetMatrix());
    DeferCreate(blas);

    MeshRaytracingData* meshRaytracingData = new MeshRaytracingData;

    for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
    {
        meshRaytracingData->bottomLevelAccelerationStructures[frameIndex] = blas;
    }

    meshComponent.raytracingData = meshRaytracingData;

    if (!GetWorld())
    {
        return;
    }

    PUSH_RENDER_COMMAND(AddBLASToTLAS, TResourceHandle<RenderWorld>(GetWorld()->GetRenderResource()), meshComponent.raytracingData->bottomLevelAccelerationStructures);

    GetEntityManager().RemoveTag<EntityTag::UPDATE_BLAS>(entity);
}

void BLASUpdaterSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    MeshComponent& meshComponent = GetEntityManager().GetComponent<MeshComponent>(entity);

    if (!meshComponent.raytracingData)
    {
        return;
    }

    PUSH_RENDER_COMMAND(RemoveBLASFromTLAS, TResourceHandle<RenderWorld>(GetWorld()->GetRenderResource()), meshComponent.raytracingData->bottomLevelAccelerationStructures);

    for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
    {
        BLASRef& blas = meshComponent.raytracingData->bottomLevelAccelerationStructures[frameIndex];

        if (blas.IsValid())
        {
            SafeRelease(std::move(blas));
        }
    }

    delete meshComponent.raytracingData;
    meshComponent.raytracingData = nullptr;
}

void BLASUpdaterSystem::Process(float delta)
{
    if (!g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.enabled").ToBool())
    {
        return;
    }

    HashSet<WeakHandle<Entity>> updatedEntities;

    for (auto [entity, meshComponent, transformComponent, _] : GetEntityManager().GetEntitySet<MeshComponent, TransformComponent, EntityTagComponent<EntityTag::UPDATE_BLAS>>().GetScopedView(GetComponentInfos()))
    {
        if (!meshComponent.raytracingData)
        {
            continue;
        }

        /// vvv FIXME
        // if (blasComponent.blas->GetMesh() != meshComponent.mesh) {
        //     blasComponent.blas->SetMesh(meshComponent.mesh);
        // }

        // if (blasComponent.blas->GetMaterial() != meshComponent.material) {
        //     blasComponent.blas->SetMaterial(meshComponent.material);
        // }

        PUSH_RENDER_COMMAND(UpdateBLASTransform, meshComponent.raytracingData->bottomLevelAccelerationStructures, transformComponent.transform.GetMatrix());

        updatedEntities.Insert(entity->WeakHandleFromThis());
    }

    if (updatedEntities.Any())
    {
        AfterProcess([this, updatedEntities = std::move(updatedEntities)]()
            {
                for (const WeakHandle<Entity>& entityWeak : updatedEntities)
                {
                    GetEntityManager().RemoveTag<EntityTag::UPDATE_BLAS>(entityWeak.GetUnsafe());
                }
            });
    }
}

} // namespace hyperion
