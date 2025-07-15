/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/EntityRenderProxySystem_Mesh.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/animation/Skeleton.hpp>

#include <scene/Scene.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>

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

    if (!meshComponent.mesh.IsValid() || !meshComponent.material.IsValid())
    {
        if (!meshComponent.mesh.IsValid())
        {
            HYP_LOG(ECS, Warning, "Mesh not valid for entity #{}!", entity->Id());
        }

        if (!meshComponent.material.IsValid())
        {
            HYP_LOG(ECS, Warning, "Material not valid for entity #{}!", entity->Id());
        }

        return;
    }

    entity->SetNeedsRenderProxyUpdate();

    TransformComponent* transformComponent = GetEntityManager().TryGetComponent<TransformComponent>(entity);

    if (transformComponent && meshComponent.previousModelMatrix == transformComponent->transform.GetMatrix())
    {
        GetEntityManager().RemoveTag<EntityTag::UPDATE_RENDER_PROXY>(entity);
    }
    else
    {
        meshComponent.previousModelMatrix = transformComponent->transform.GetMatrix();
    }
}

void EntityRenderProxySystem_Mesh::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);
}

void EntityRenderProxySystem_Mesh::Process(float delta)
{
    HashSet<WeakHandle<Entity>> updatedEntities;

    for (auto [entity, meshComponent, transformComponent, boundingBoxComponent, _] : GetEntityManager().GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent, EntityTagComponent<EntityTag::UPDATE_RENDER_PROXY>>().GetScopedView(GetComponentInfos()))
    {
        HYP_NAMED_SCOPE_FMT("Update draw data for entity #{}", entity->Id());

        if (!meshComponent.mesh.IsValid() || !meshComponent.material.IsValid())
        {
            HYP_LOG(ECS, Warning, "Mesh or material not valid for entity #{}!", entity->Id());

            updatedEntities.Insert(entity->WeakHandleFromThis());

            continue;
        }

        entity->SetNeedsRenderProxyUpdate();

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
}

} // namespace hyperion
