/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/BLASUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/Scene.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <scene/World.hpp>

#include <rendering/RenderWorld.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/Deferred.hpp>

#include <rendering/RenderCommand.hpp>
#include <rendering/rt/RenderAccelerationStructure.hpp>

#include <core/containers/HashSet.hpp>

#include <core/logging/Logger.hpp>

#include <system/AppContext.hpp>

#include <EngineGlobals.hpp>
#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

#if 0
#pragma region Render commands

struct RENDER_COMMAND(UpdateBLASTransform)
    : RenderCommand
{
    FixedArray<BLASRef, g_framesInFlight> bottomLevelAccelerationStructures;
    Matrix4 transform;

    RENDER_COMMAND(UpdateBLASTransform)(
        const FixedArray<BLASRef, g_framesInFlight>& bottomLevelAccelerationStructures,
        const Matrix4& transform)
        : bottomLevelAccelerationStructures(bottomLevelAccelerationStructures),
          transform(transform)
    {
    }

    virtual ~RENDER_COMMAND(UpdateBLASTransform)() override = default;

    virtual RendererResult operator()() override
    {
        for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
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

struct RENDER_COMMAND(AddBLASToTLAS) : RenderCommand
{
    TResourceHandle<RenderWorld> renderWorld;
    FixedArray<BLASRef, g_framesInFlight> bottomLevelAccelerationStructures;

    RENDER_COMMAND(AddBLASToTLAS)(
        const TResourceHandle<RenderWorld>& renderWorld,
        const FixedArray<BLASRef, g_framesInFlight>& bottomLevelAccelerationStructures)
        : renderWorld(renderWorld),
          bottomLevelAccelerationStructures(bottomLevelAccelerationStructures)
    {
    }

    virtual ~RENDER_COMMAND(AddBLASToTLAS)() override = default;

    virtual RendererResult operator()() override
    {
        RenderEnvironment* environment = renderWorld->GetEnvironment();
        Assert(environment != nullptr);

        for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
        {
            if (bottomLevelAccelerationStructures[frameIndex].IsValid())
            {
                // TEMPORARY STOPGAP HACK!!!!!
                for (const TResourceHandle<RenderView>& renderView : renderWorld->GetViews())
                {
                    if (renderView->GetGBuffer() != nullptr)
                    {
                        DeferredRenderer* deferredRenderer = static_cast<DeferredRenderer*>(g_renderGlobalState->mainRenderer);
                        DeferredPassData* lastFramePassData = deferredRenderer->GetLastFrameData().GetPassDataForView(renderView->GetView());

                        AssertDebug(lastFramePassData != nullptr);

                        TLASRef& tlas = lastFramePassData->topLevelAccelerationStructures[frameIndex];
                        AssertDebug(tlas);

                        tlas->AddBLAS(bottomLevelAccelerationStructures[frameIndex]);
                    }
                }
            }
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(RemoveBLASFromTLAS) : RenderCommand
{
    TResourceHandle<RenderWorld> renderWorld;
    FixedArray<BLASRef, g_framesInFlight> bottomLevelAccelerationStructures;

    RENDER_COMMAND(RemoveBLASFromTLAS)(
        const TResourceHandle<RenderWorld>& renderWorld,
        const FixedArray<BLASRef, g_framesInFlight>& bottomLevelAccelerationStructures)
        : renderWorld(renderWorld),
          bottomLevelAccelerationStructures(bottomLevelAccelerationStructures)
    {
    }

    virtual ~RENDER_COMMAND(RemoveBLASFromTLAS)() override = default;

    virtual RendererResult operator()() override
    {
        RenderEnvironment* environment = renderWorld->GetEnvironment();
        Assert(environment != nullptr);

        for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
        {
            if (bottomLevelAccelerationStructures[frameIndex].IsValid())
            {
                // TEMPORARY STOPGAP HACK!!!!!
                for (const TResourceHandle<RenderView>& renderView : renderWorld->GetViews())
                {
                    if (renderView->GetGBuffer() != nullptr)
                    {
                        DeferredRenderer* deferredRenderer = static_cast<DeferredRenderer*>(g_renderGlobalState->mainRenderer);
                        DeferredPassData* lastFramePassData = deferredRenderer->GetLastFrameData().GetPassDataForView(renderView->GetView());

                        AssertDebug(lastFramePassData != nullptr);

                        TLASRef& tlas = lastFramePassData->topLevelAccelerationStructures[frameIndex];
                        AssertDebug(tlas);

                        tlas->RemoveBLAS(bottomLevelAccelerationStructures[frameIndex]);
                    }
                }
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

    Assert(meshComponent.proxy != nullptr);

    InitObject(meshComponent.mesh);
    Assert(meshComponent.mesh->IsReady());

    InitObject(meshComponent.material);
    Assert(meshComponent.material->IsReady());

    BLASRef blas;

    {
        blas = meshComponent.mesh->BuildBLAS(meshComponent.material);

        if (!blas)
        {
            HYP_LOG(Rendering, Error, "Failed to build BLAS for mesh #{} ({})", meshComponent.mesh.Id().Value(), meshComponent.mesh->GetName());

            return;
        }
    }

    blas->SetTransform(transformComponent.transform.GetMatrix());
    DeferCreate(blas);

    MeshRaytracingData& meshRaytracingData = meshComponent.proxy->raytracingData;

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        meshRaytracingData.bottomLevelAccelerationStructures[frameIndex] = blas;
    }

    if (!GetWorld())
    {
        return;
    }

    //PUSH_RENDER_COMMAND(AddBLASToTLAS, TResourceHandle<RenderWorld>(GetWorld()->GetRenderResource()), meshComponent.proxy->raytracingData->bottomLevelAccelerationStructures);

    GetEntityManager().RemoveTag<EntityTag::UPDATE_BLAS>(entity);
}

void BLASUpdaterSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    MeshComponent& meshComponent = GetEntityManager().GetComponent<MeshComponent>(entity);

    if (!meshComponent.proxy)
    {
        return;
    }

    //PUSH_RENDER_COMMAND(RemoveBLASFromTLAS, TResourceHandle<RenderWorld>(GetWorld()->GetRenderResource()), meshComponent.proxy->raytracingData->bottomLevelAccelerationStructures);

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
        if (!meshComponent.proxy)
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

        //PUSH_RENDER_COMMAND(UpdateBLASTransform, meshComponent.raytracingData->bottomLevelAccelerationStructures, transformComponent.transform.GetMatrix());

        //updatedEntities.Insert(entity->WeakHandleFromThis());
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
#endif
} // namespace hyperion
