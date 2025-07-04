/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/EntityMeshDirtyStateSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/Scene.hpp>
#include <scene/Mesh.hpp>
#include <scene/Material.hpp>
#include <scene/BVH.hpp>

#include <core/containers/HashSet.hpp>

#include <rendering/backend/RenderCommand.hpp>

namespace hyperion {

void EntityMeshDirtyStateSystem::OnEntityAdded(const Handle<Entity>& entity)
{
    SystemBase::OnEntityAdded(entity);

    MeshComponent& mesh_component = GetEntityManager().GetComponent<MeshComponent>(entity);

    InitObject(mesh_component.mesh);
    InitObject(mesh_component.material);

    GetEntityManager().AddTag<EntityTag::UPDATE_RENDER_PROXY>(entity);
}

void EntityMeshDirtyStateSystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);
}

void EntityMeshDirtyStateSystem::Process(float delta)
{
    HashSet<ID<Entity>> updated_entity_ids;

    for (auto [entity_id, mesh_component, transform_component] : GetEntityManager().GetEntitySet<MeshComponent, TransformComponent>().GetScopedView(GetComponentInfos()))
    {
        // Update the material
        if (mesh_component.material.IsValid() && mesh_component.material->GetMutationState().IsDirty())
        {
            mesh_component.material->EnqueueRenderUpdates();
        }

        // If transform has changed, mark the MeshComponent as dirty
        if (mesh_component.previous_model_matrix != transform_component.transform.GetMatrix())
        {
            updated_entity_ids.Insert(entity_id);
        }
    }

    if (updated_entity_ids.Any())
    {
        AfterProcess([this, entity_ids = std::move(updated_entity_ids)]()
            {
                for (const ID<Entity>& entity_id : entity_ids)
                {
                    GetEntityManager().AddTag<EntityTag::UPDATE_RENDER_PROXY>(entity_id);
                }
            });
    }
}

} // namespace hyperion
