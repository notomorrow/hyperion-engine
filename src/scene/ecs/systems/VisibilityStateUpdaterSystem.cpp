/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/VisibilityStateUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/Scene.hpp>
#include <scene/Octree.hpp>

#include <core/containers/HashSet.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion {

void VisibilityStateUpdaterSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    VisibilityStateComponent& visibilityStateComponent = GetEntityManager().GetComponent<VisibilityStateComponent>(entity);

    GetEntityManager().AddTag<EntityTag::UPDATE_VISIBILITY_STATE>(entity);

    if (visibilityStateComponent.octantId != OctantId::Invalid())
    {
        return;
    }

    visibilityStateComponent.visibilityState = nullptr;

    // This system must be ran before WorldAABBUpdaterSystem so that the bounding box is up to date

    BoundingBoxComponent& boundingBoxComponent = GetEntityManager().GetComponent<BoundingBoxComponent>(entity);

    Octree& octree = GetEntityManager().GetScene()->GetOctree();

    const Octree::InsertResult insertResult = octree.Insert(entity, boundingBoxComponent.worldAabb);

    if (insertResult.first)
    {
        Assert(insertResult.second != OctantId::Invalid(), "Invalid octant Id returned from Insert()");

        visibilityStateComponent.octantId = insertResult.second;
        visibilityStateComponent.visibilityState = nullptr;

        if (Octree* octant = octree.GetChildOctant(visibilityStateComponent.octantId))
        {
            visibilityStateComponent.visibilityState = &octant->GetVisibilityState();
        }

        // HYP_LOG(Octree, Debug, "Inserted entity #{} into octree, inserted at {}, {}", entity.Id().Value(), visibilityStateComponent.octantId.GetIndex(), visibilityStateComponent.octantId.GetDepth());

        // GetEntityManager().RemoveTag<EntityTag::UPDATE_VISIBILITY_STATE>(entity);
    }
    else
    {
        HYP_LOG(Octree, Warning, "Failed to insert entity #{} into octree: {}", entity->Id(), insertResult.first.message);
    }
}

void VisibilityStateUpdaterSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    VisibilityStateComponent& visibilityStateComponent = GetEntityManager().GetComponent<VisibilityStateComponent>(entity);

    Octree& octree = GetEntityManager().GetScene()->GetOctree();

    const Octree::Result removeResult = octree.Remove(entity);

    if (!removeResult)
    {
        HYP_LOG(Octree, Warning, "Failed to remove Entity #{} from octree: {}", entity->Id(), removeResult.message);
    }

    visibilityStateComponent.octantId = OctantId::Invalid();
    visibilityStateComponent.visibilityState = nullptr;
}

void VisibilityStateUpdaterSystem::Process(float delta)
{
    Octree& octree = GetEntityManager().GetScene()->GetOctree();

    HashSet<WeakHandle<Entity>> updatedEntities;

    const auto updateVisbilityState = [&octree, &updatedEntities](Entity* entity, VisibilityStateComponent& visibilityStateComponent, BoundingBoxComponent& boundingBoxComponent)
    {
        bool needsOctreeUpdate = false;

        const bool visibilityStateInvalidated = visibilityStateComponent.flags & VISIBILITY_STATE_FLAG_INVALIDATED;

        needsOctreeUpdate |= visibilityStateInvalidated;

        visibilityStateComponent.flags &= ~VISIBILITY_STATE_FLAG_INVALIDATED;

        // if entity is not in the octree, try to insert it
        if (visibilityStateComponent.octantId == OctantId::Invalid())
        {
            visibilityStateComponent.visibilityState = nullptr;

            if (!boundingBoxComponent.worldAabb.IsValid())
            {
                return;
            }

            const Octree::InsertResult insertResult = octree.Insert(entity, boundingBoxComponent.worldAabb);

            if (insertResult.first)
            {
                Assert(insertResult.second != OctantId::Invalid(), "Invalid octant Id returned from Insert()");

                visibilityStateComponent.octantId = insertResult.second;

                if (Octree* octant = octree.GetChildOctant(visibilityStateComponent.octantId))
                {
                    visibilityStateComponent.visibilityState = &octant->GetVisibilityState();
                }
            }

            return;
        }

        if (needsOctreeUpdate)
        {
            visibilityStateComponent.visibilityState = nullptr;

            // force entry invalidation if the bounding box is not finite,
            // so directional lights changing cause the entire octree to be updated.
            const bool forceEntryInvalidation = visibilityStateInvalidated;

            const Octree::InsertResult updateResult = octree.Update(entity, boundingBoxComponent.worldAabb, forceEntryInvalidation);

            visibilityStateComponent.octantId = updateResult.second;

            if (!updateResult.first)
            {
                HYP_LOG(Octree, Warning, "Failed to update entity {} in octree: {}", entity->Id(), updateResult.first.message);

                return;
            }

            Assert(updateResult.second != OctantId::Invalid(), "Invalid octant Id returned from Update()");
        }

        if (visibilityStateComponent.octantId != OctantId::Invalid())
        {
            visibilityStateComponent.visibilityState = nullptr;

            if (Octree* octant = octree.GetChildOctant(visibilityStateComponent.octantId))
            {
                visibilityStateComponent.visibilityState = &octant->GetVisibilityState();
            }
        }

        updatedEntities.Insert(entity->WeakHandleFromThis());
    };

    for (auto [entity, visibilityStateComponent, boundingBoxComponent, _] : GetEntityManager().GetEntitySet<VisibilityStateComponent, BoundingBoxComponent, EntityTagComponent<EntityTag::UPDATE_VISIBILITY_STATE>>().GetScopedView(GetComponentInfos()))
    {
        updateVisbilityState(entity, visibilityStateComponent, boundingBoxComponent);
    }

    if (updatedEntities.Any())
    {
        AfterProcess([this, updatedEntities = std::move(updatedEntities)]()
            {
                for (const WeakHandle<Entity>& entityWeak : updatedEntities)
                {
                    GetEntityManager().RemoveTag<EntityTag::UPDATE_VISIBILITY_STATE>(entityWeak.GetUnsafe());
                }
            });
    }
}

} // namespace hyperion
