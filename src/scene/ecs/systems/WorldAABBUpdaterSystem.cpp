/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/WorldAABBUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/Scene.hpp>

namespace hyperion {

EnumFlags<SceneFlags> WorldAABBUpdaterSystem::GetRequiredSceneFlags() const
{
    return SceneFlags::NONE;
}

void WorldAABBUpdaterSystem::OnEntityAdded(const Handle<Entity> &entity)
{
    SystemBase::OnEntityAdded(entity);

    BoundingBoxComponent &bounding_box_component = GetEntityManager().GetComponent<BoundingBoxComponent>(entity);
    TransformComponent &transform_component = GetEntityManager().GetComponent<TransformComponent>(entity);

    bounding_box_component.world_aabb = BoundingBox::Empty();

    if (bounding_box_component.local_aabb.IsValid()) {
        for (const Vec3f &corner : bounding_box_component.local_aabb.GetCorners()) {
            bounding_box_component.world_aabb = bounding_box_component.world_aabb.Union(transform_component.transform.GetMatrix() * corner);
        }
    }

    bounding_box_component.transform_hash_code = transform_component.transform.GetHashCode();

    // @TODO Use an EntityTag to tell the MeshComponent that it needs to be updated, rather than having to mutate that component
    // EntityTags will need to be able to be used across threads
    MeshComponent &mesh_component = GetEntityManager().GetComponent<MeshComponent>(entity);
    mesh_component.flags |= MESH_COMPONENT_FLAG_DIRTY;
}

void WorldAABBUpdaterSystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);
}

void WorldAABBUpdaterSystem::Process(GameCounter::TickUnit delta)
{
    for (auto [entity_id, bounding_box_component, transform_component, mesh_component] : GetEntityManager().GetEntitySet<BoundingBoxComponent, TransformComponent, MeshComponent>().GetScopedView(GetComponentInfos())) {
        const BoundingBox local_aabb = bounding_box_component.local_aabb;
        BoundingBox world_aabb = bounding_box_component.world_aabb;

        const HashCode transform_hash_code = transform_component.transform.GetHashCode();

        if (transform_hash_code != bounding_box_component.transform_hash_code) {
            world_aabb = BoundingBox::Empty();

            if (local_aabb.IsValid()) {
                for (const Vec3f &corner : local_aabb.GetCorners()) {
                    world_aabb = world_aabb.Union(transform_component.transform.GetMatrix() * corner);
                }
            }

            bounding_box_component.local_aabb = local_aabb;
            bounding_box_component.world_aabb = world_aabb;
            bounding_box_component.transform_hash_code = transform_hash_code;
        
            // Update the render proxy
            mesh_component.flags |= MESH_COMPONENT_FLAG_DIRTY;
        }
    }
}

} // namespace hyperion