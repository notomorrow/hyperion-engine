/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/systems/WorldAABBUpdaterSystem.hpp>
#include <scene/EntityManager.hpp>

#include <scene/Scene.hpp>

#include <core/containers/HashSet.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Entity);

void WorldAABBUpdaterSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    if (ProcessEntity(entity, GetEntityManager().GetComponent<BoundingBoxComponent>(entity), GetEntityManager().GetComponent<TransformComponent>(entity)))
    {
        GetEntityManager().AddTags<EntityTag::UPDATE_RENDER_PROXY, EntityTag::UPDATE_VISIBILITY_STATE>(entity);
    }
    
    GetEntityManager().RemoveTag<EntityTag::UPDATE_AABB>(entity);
}

void WorldAABBUpdaterSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);
}

void WorldAABBUpdaterSystem::Process(float delta)
{
    HashMap<WeakHandle<Entity>, bool> updatedEntities;

    for (auto [entity, boundingBoxComponent, transformComponent, _] : GetEntityManager().GetEntitySet<BoundingBoxComponent, TransformComponent, EntityTagComponent<EntityTag::UPDATE_AABB>>().GetScopedView(GetComponentInfos()))
    {
        const bool wasWorldAabbChanged = ProcessEntity(entity, boundingBoxComponent, transformComponent);
        
        updatedEntities[entity->WeakHandleFromThis()] = wasWorldAabbChanged;
    }

    if (updatedEntities.Any())
    {
        AfterProcess([this, updatedEntities = std::move(updatedEntities)]()
            {
                for (const auto& [entityWeak, wasWorldAabbChanged] : updatedEntities)
                {
                    Entity* entity = entityWeak.GetUnsafe(); // don't use ptr so it's fine to use GetUnsafe()
                    
                    if (wasWorldAabbChanged)
                    {
                        GetEntityManager().AddTags<EntityTag::UPDATE_RENDER_PROXY, EntityTag::UPDATE_VISIBILITY_STATE>(entity);
                    }

                    GetEntityManager().RemoveTag<EntityTag::UPDATE_AABB>(entity);
                }
            });
    }
}

//! Return true on change
bool WorldAABBUpdaterSystem::ProcessEntity(Entity* entity, BoundingBoxComponent& boundingBoxComponent, TransformComponent& transformComponent)
{
    const BoundingBox prevWorldAabb = boundingBoxComponent.worldAabb;
    const BoundingBox& localAabb = boundingBoxComponent.localAabb;
    BoundingBox& worldAabb = boundingBoxComponent.worldAabb;

    worldAabb = BoundingBox::Empty();

    if (localAabb.IsValid())
    {
        for (const Vec3f& corner : localAabb.GetCorners())
        {
            worldAabb = worldAabb.Union(transformComponent.transform.GetMatrix() * corner);
        }
    }
    
    if (prevWorldAabb == worldAabb)
    {
        // no change
        return false;
    }

    boundingBoxComponent.worldAabb = worldAabb;

    return true;
}

} // namespace hyperion
