/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/EntityRenderProxySystem_Mesh.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/animation/Skeleton.hpp>

#include <scene/Scene.hpp>
#include <scene/Mesh.hpp>
#include <scene/Material.hpp>

#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderMaterial.hpp>

#include <rendering/RenderCommand.hpp>

#include <core/containers/HashSet.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

void EntityRenderProxySystem_Mesh::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    MeshComponent& meshComponent = GetEntityManager().GetComponent<MeshComponent>(entity);

    InitObject(meshComponent.mesh);
    InitObject(meshComponent.material);
    InitObject(meshComponent.skeleton);

    Assert(meshComponent.proxy == nullptr);

    if (meshComponent.mesh.IsValid() && meshComponent.material.IsValid())
    {
        meshComponent.proxy = new RenderProxyMesh;
        meshComponent.proxy->entity = entity->WeakHandleFromThis();
        meshComponent.proxy->mesh = meshComponent.mesh;
        meshComponent.proxy->material = meshComponent.material;
        meshComponent.proxy->skeleton = meshComponent.skeleton;
        meshComponent.proxy->instanceData = meshComponent.instanceData;
        meshComponent.proxy->version = 0;
        meshComponent.proxy->bufferData.bucket = meshComponent.material.IsValid() ? meshComponent.material->GetRenderAttributes().bucket : RB_NONE;
        meshComponent.proxy->bufferData.flags = meshComponent.skeleton.IsValid() ? ENTITY_GPU_FLAG_HAS_SKELETON : ENTITY_GPU_FLAG_NONE;
        meshComponent.proxy->bufferData.modelMatrix = Matrix4::Identity();
        meshComponent.proxy->bufferData.previousModelMatrix = Matrix4::Identity();
        meshComponent.proxy->bufferData.worldAabbMax = BoundingBox::Empty().max;
        meshComponent.proxy->bufferData.worldAabbMin = BoundingBox::Empty().min;
        meshComponent.proxy->bufferData.userData = reinterpret_cast<EntityShaderData::EntityUserData&>(meshComponent.userData);

        // PUSH_RENDER_COMMAND(UpdateEntityDrawData, Array<RenderProxyMesh*> { meshComponent.proxy });

        GetEntityManager().RemoveTag<EntityTag::UPDATE_RENDER_PROXY>(entity);
    }
    else
    {
        if (!meshComponent.mesh.IsValid())
        {
            HYP_LOG(ECS, Warning, "Mesh not valid for entity #{}!", entity->Id());
        }

        if (!meshComponent.material.IsValid())
        {
            HYP_LOG(ECS, Warning, "Material not valid for entity #{}!", entity->Id());
        }
    }
}

void EntityRenderProxySystem_Mesh::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    MeshComponent& meshComponent = GetEntityManager().GetComponent<MeshComponent>(entity);

    if (meshComponent.proxy)
    {
        for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
        {
            BLASRef& blas = meshComponent.proxy->raytracingData.bottomLevelAccelerationStructures[frameIndex];

            if (blas.IsValid())
            {
                SafeRelease(std::move(blas));
            }
        }

        delete meshComponent.proxy;
        meshComponent.proxy = nullptr;
    }
}

void EntityRenderProxySystem_Mesh::Process(float delta)
{
    HashSet<WeakHandle<Entity>> updatedEntities;
    Array<RenderProxyMesh*> renderProxyPtrs;

    for (auto [entity, meshComponent, transformComponent, boundingBoxComponent, _] : GetEntityManager().GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent, EntityTagComponent<EntityTag::UPDATE_RENDER_PROXY>>().GetScopedView(GetComponentInfos()))
    {
        HYP_NAMED_SCOPE_FMT("Update draw data for entity #{}", entity->Id());

        if (!meshComponent.mesh.IsValid() || !meshComponent.material.IsValid())
        {
            if (meshComponent.proxy)
            {
                HYP_LOG(ECS, Warning, "Mesh or material not valid for entity #{}!", entity->Id());

                delete meshComponent.proxy;
                meshComponent.proxy = nullptr;

                updatedEntities.Insert(entity->WeakHandleFromThis());
            }

            continue;
        }

        if (!meshComponent.proxy)
        {
            meshComponent.proxy = new RenderProxyMesh {};
        }

        const uint32 renderProxyVersion = meshComponent.proxy->version + 1;

        // Update MeshComponent's proxy
        // @TODO: Include RT info on RenderProxy, add a system that will update BLAS on the render thread.
        // @TODO Add Lightmap volume info
        RenderProxyMesh& proxy = *meshComponent.proxy;

        proxy.entity = entity->WeakHandleFromThis();
        proxy.mesh = meshComponent.mesh;
        proxy.material = meshComponent.material;
        proxy.skeleton = meshComponent.skeleton;
        proxy.instanceData = meshComponent.instanceData;
        proxy.version = renderProxyVersion;

        proxy.bufferData.modelMatrix = transformComponent.transform.GetMatrix();
        proxy.bufferData.previousModelMatrix = meshComponent.previousModelMatrix;
        proxy.bufferData.worldAabbMax = boundingBoxComponent.worldAabb.max;
        proxy.bufferData.worldAabbMin = boundingBoxComponent.worldAabb.min;
        proxy.bufferData.userData = reinterpret_cast<EntityShaderData::EntityUserData&>(meshComponent.userData);

        renderProxyPtrs.PushBack(meshComponent.proxy);

        if (meshComponent.previousModelMatrix == transformComponent.transform.GetMatrix())
        {
            updatedEntities.Insert(entity->WeakHandleFromThis());
        }
        else
        {
            meshComponent.previousModelMatrix = transformComponent.transform.GetMatrix();
        }
    }

    if (updatedEntities.Any())
    {
        AfterProcess([this, updatedEntities = std::move(updatedEntities)]()
            {
                for (const WeakHandle<Entity>& entityWeak : updatedEntities)
                {
                    GetEntityManager().RemoveTag<EntityTag::UPDATE_RENDER_PROXY>(entityWeak.GetUnsafe());
                }
            });
    }

    if (renderProxyPtrs.Any())
    {
        // PUSH_RENDER_COMMAND(UpdateEntityDrawData, std::move(renderProxyPtrs));
    }
}

} // namespace hyperion
