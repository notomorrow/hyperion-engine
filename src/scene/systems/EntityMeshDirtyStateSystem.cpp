/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/systems/EntityMeshDirtyStateSystem.hpp>
#include <scene/EntityManager.hpp>

#include <scene/Scene.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <scene/BVH.hpp>

#include <core/containers/HashSet.hpp>

#include <rendering/RenderCommand.hpp>

namespace hyperion {

void EntityMeshDirtyStateSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    MeshComponent& meshComponent = GetEntityManager().GetComponent<MeshComponent>(entity);

    InitObject(meshComponent.mesh);
    InitObject(meshComponent.material);

    GetEntityManager().AddTag<EntityTag::UPDATE_RENDER_PROXY>(entity);
}

void EntityMeshDirtyStateSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);
}

void EntityMeshDirtyStateSystem::Process(float delta)
{
    HashSet<WeakHandle<Entity>> updatedEntities;

    /*for (auto [entity, meshComponent, transformComponent] : GetEntityManager().GetEntitySet<MeshComponent, TransformComponent>().GetScopedView(GetComponentInfos()))
    {
        // Update the material
        if (meshComponent.material.IsValid() && meshComponent.material->GetMutationState().IsDirty())
        {
            meshComponent.material->EnqueueRenderUpdates();
        }

        // If transform has changed, mark the MeshComponent as dirty
        if (meshComponent.previousModelMatrix != transformComponent.transform.GetMatrix())
        {
            updatedEntities.Insert(entity->WeakHandleFromThis());
        }
    }

    if (updatedEntities.Any())
    {
        AfterProcess([this, updatedEntities = std::move(updatedEntities)]()
            {
                for (const WeakHandle<Entity>& entityWeak : updatedEntities)
                {
                    GetEntityManager().AddTag<EntityTag::UPDATE_RENDER_PROXY>(entityWeak.GetUnsafe());
                }
            });
    }*/
}

} // namespace hyperion
