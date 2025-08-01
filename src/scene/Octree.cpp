/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/Octree.hpp>
#include <scene/Entity.hpp>
#include <scene/Node.hpp>
#include <rendering/Mesh.hpp>
#include <scene/BVH.hpp>

#include <scene/EntityManager.hpp>
#include <scene/components/VisibilityStateComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/NodeLinkComponent.hpp>
#include <scene/components/MeshComponent.hpp>

#include <scene/camera/Camera.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

Octree::Octree(const Handle<EntityManager>& entityManager)
    : OctreeBase(),
      m_entityManager(entityManager)
{
}

Octree::Octree(const Handle<EntityManager>& entityManager, const BoundingBox& aabb)
    : OctreeBase(aabb),
      m_entityManager(entityManager)
{
}

Octree::Octree(const Handle<EntityManager>& entityManager, const BoundingBox& aabb, Octree* parent, uint8 index)
    : OctreeBase(aabb, parent, index),
      m_entityManager(entityManager)
{
}

Octree::~Octree()
{
    // to ensure VisibilityStateComponents are updated
    Clear();
}

void Octree::SetEntityManager(const Handle<EntityManager>& entityManager)
{
    HYP_SCOPE;

    m_entityManager = entityManager;

    if (IsDivided())
    {
        for (Octant& octant : m_octants)
        {
            Assert(octant.octree != nullptr);

            static_cast<Octree*>(octant.octree.Get())->SetEntityManager(m_entityManager);
        }
    }
}

void Octree::Collect(Array<Entity*>& outEntities) const
{
    outEntities.Reserve(outEntities.Size() + m_payload.entries.Size());

    for (const auto& entry : m_payload.entries)
    {
        outEntities.PushBack(entry.value);
    }

    if (m_isDivided)
    {
        for (const Octant& octant : m_octants)
        {
            Assert(octant.octree != nullptr);

            static_cast<Octree*>(octant.octree.Get())->Collect(outEntities);
        }
    }
}

void Octree::Collect(const BoundingSphere& bounds, Array<Entity*>& outEntities) const
{
    if (!bounds.Overlaps(m_aabb))
    {
        return;
    }

    outEntities.Reserve(outEntities.Size() + m_payload.entries.Size());

    for (const auto& entry : m_payload.entries)
    {
        if (bounds.Overlaps(entry.aabb))
        {
            outEntities.PushBack(entry.value);
        }
    }

    if (m_isDivided)
    {
        for (const Octant& octant : m_octants)
        {
            Assert(octant.octree != nullptr);

            octant.octree->Collect(bounds, outEntities);
        }
    }
}

void Octree::Collect(const BoundingBox& bounds, Array<Entity*>& outEntities) const
{
    if (!m_aabb.Overlaps(bounds))
    {
        return;
    }

    outEntities.Reserve(outEntries.Size() + m_payload.entries.Size());

    for (const auto& entry : m_payload.entries)
    {
        if (bounds.Overlaps(entry.aabb))
        {
            outEntities.PushBack(entry.value);
        }
    }

    if (m_isDivided)
    {
        for (const Octant& octant : m_octants)
        {
            Assert(octant.octree != nullptr);

            octant.octree->Collect(bounds, outEntities);
        }
    }
}

void Octree::Clear()
{
    HYP_SCOPE;

    Array<SceneOctreePayload> payloads;
    OctreeBase::Clear(payloads, /* undivide */ true);

    if (m_entityManager)
    {
        Assert(Threads::IsOnThread(m_entityManager->GetOwnerThreadId()));

        for (SceneOctreePayload& payload : payloads)
        {
            for (auto& entry : payload.entries)
            {
                Entity* entity = entry.value;

                if (!entity)
                {
                    continue;
                }

                if (VisibilityStateComponent* visibilityStateComponent = m_entityManager->TryGetComponent<VisibilityStateComponent>(entity))
                {
                    visibilityStateComponent->octantId = OctantId::Invalid();
                    visibilityStateComponent->visibilityState = nullptr;
                }

                m_entityManager->AddTag<EntityTag::UPDATE_VISIBILITY_STATE>(entity);
            }
        }
    }

    RebuildEntriesHash();
}

Octree::Result Octree::Rebuild(const BoundingBox& newAabb, bool allowGrow)
{
    Array<SceneOctreePayload> payloads;
    OctreeBase::Clear(payloads, /* undivide */ true);

    m_aabb = newAabb;

    if (allowGrow)
    {
        Assert(IsRoot());

        for (auto& payload : payloads)
        {
            for (auto& entry : payload.entries)
            {
                if (entry.aabb.IsValid() && entry.aabb.IsFinite())
                {
                    m_aabb = m_aabb.Union(entry.aabb);
                }
            }
        }
    }

    InitOctants();

    for (auto& payload : payloads)
    {
        for (auto& entry : payload.entries)
        {
            Entity* entity = entry.value;
            Assert(entity != nullptr);

            if (entry.aabb.IsValid() && entry.aabb.IsFinite())
            {
                AssertDebug(m_aabb.Contains(entry.aabb));
            }

            OctreeBase::Result insertResult = Insert(entity, entry.aabb, true /* allow rebuild */);

            if (insertResult.HasError())
            {
                return insertResult;
            }

            VisibilityStateComponent* visibilityStateComponent = m_entityManager->TryGetComponent<VisibilityStateComponent>(entity);

            if (visibilityStateComponent)
            {
                visibilityStateComponent->octantId = insertResult.GetValue();
                visibilityStateComponent->visibilityState = nullptr;
            }
            else
            {
                m_entityManager->AddComponent<VisibilityStateComponent>(entity, VisibilityStateComponent { .octantId = insertResult.GetValue(), .visibilityState = nullptr });
            }

            m_entityManager->AddTag<EntityTag::UPDATE_VISIBILITY_STATE>(entity);
        }
    }

    return m_octantId;
}

void Octree::PerformUpdates()
{
    HYP_SCOPE;

    Assert(m_state != nullptr);

    if (!m_state->IsDirty())
    {
        // No octant to rebuild, skipping
        return;
    }

    OctreeBase::PerformUpdates();

    RebuildEntriesHash();
}

void Octree::NextVisibilityState()
{
    HYP_SCOPE;

    Assert(IsRoot());

    m_visibilityState.Next();
}

void Octree::CalculateVisibility(const Handle<Camera>& camera)
{
    HYP_SCOPE;

    Assert(IsRoot());

    UpdateVisibilityState(camera, m_visibilityState.validityMarker);
}

void Octree::UpdateVisibilityState(const Handle<Camera>& camera, uint16 validityMarker)
{
    if (!camera.IsValid())
    {
        return;
    }

    const Frustum& frustum = camera->GetFrustum();

    if (!frustum.ContainsAABB(m_aabb))
    {
        return;
    }

    Octree* current = this;
    uint8 childIndex = uint8(-1);

    while (true)
    {
        // Process current node.
        current->m_visibilityState.validityMarker = validityMarker;
        current->m_visibilityState.MarkAsValid(camera.Id());

        if (current->m_isDivided)
        {
            bool descended = false;

            for (uint8 i = childIndex + 1; i < 8; i++)
            {
                if (!frustum.ContainsAABB(current->m_octants[i].aabb))
                {
                    continue;
                }

                current = static_cast<Octree*>(current->m_octants[i].octree.Get());
                childIndex = uint8(-1);

                descended = true;

                break;
            }
            if (descended)
            {
                continue;
            }
        }

        if (current->m_parent == nullptr)
        {
            break;
        }

        childIndex = current->m_octantId.GetIndex();
        current = static_cast<Octree*>(current->m_parent);
    }
}

void Octree::ResetEntriesHash()
{
    HYP_SCOPE;

    m_entryHashes = {};
}

void Octree::RebuildEntriesHash(uint32 level)
{
    HYP_SCOPE;

    ResetEntriesHash();

    for (const auto& entry : m_payload.entries)
    {
        const HashCode entryHashCode = entry.GetHashCode();
        m_entryHashes[0].Add(entryHashCode);

        uint32 tagsMask = 0;

        if (m_entityManager)
        {
            tagsMask = m_entityManager->GetSavableTagsMask(entry.value);
        }

        FOR_EACH_BIT(tagsMask, i)
        {
            EntityTag tag = EntityTag(i + 1);
            AssertDebug(uint32(tag) < m_entryHashes.Size());

            // sanity check, temp.
            AssertDebug(m_entityManager->HasTag(entry.value, tag));

            m_entryHashes[uint32(tag)].Add(entryHashCode);
        }
    }

    if (m_isDivided)
    {
        for (const Octant& octant : m_octants)
        {
            Assert(octant.octree != nullptr);

            static_cast<Octree*>(octant.octree.Get())->RebuildEntriesHash(level + 1);
        }
    }

    // Update parent's hash to include this octant's hash
    if (m_parent)
    {
        for (uint32 i = 0; i < uint32(m_entryHashes.Size()); i++)
        {
            static_cast<Octree*>(m_parent)->m_entryHashes[i].Add(m_entryHashes[i]);
        }
    }
}

bool Octree::TestRay(const Ray& ray, RayTestResults& outResults, bool useBvh) const
{
    HYP_SCOPE;

    bool hasHit = false;

    if (ray.TestAABB(m_aabb))
    {
        for (const auto& entry : m_payload.entries)
        {
            RayTestResults aabbResult;

            if (useBvh)
            {
                if (m_entityManager && entry.value != nullptr)
                {
                    if (!m_entityManager->HasEntity(entry.value))
                    {
                        continue;
                    }

                    // If the entity has a BVH associated with it, use that instead of the AABB for more accuracy
                    if (MeshComponent* meshComponent = m_entityManager->TryGetComponent<MeshComponent>(entry.value);
                        meshComponent && meshComponent->mesh && meshComponent->mesh->GetBVH().IsValid())
                    {
                        Matrix4 modelMatrix = Matrix4::Identity();
                        Matrix4 normalMatrix = Matrix4::Identity();

                        Ray localSpaceRay = ray;

                        if (TransformComponent* transformComponent = m_entityManager->TryGetComponent<TransformComponent>(entry.value))
                        {
                            modelMatrix = transformComponent->transform.GetMatrix();
                            normalMatrix = modelMatrix.Transposed().Inverted();

                            localSpaceRay = modelMatrix.Inverted() * ray;
                        }

                        RayTestResults localBvhResults = meshComponent->mesh->GetBVH().TestRay(localSpaceRay);

                        if (localBvhResults.Any())
                        {
                            RayTestResults bvhResults;

                            for (RayHit hit : localBvhResults)
                            {
                                hit.id = entry.value->Id().Value();
                                hit.userData = nullptr;

                                Vec4f transformedNormal = normalMatrix * Vec4f(hit.normal, 0.0f);
                                hit.normal = transformedNormal.GetXYZ().Normalized();

                                Vec4f transformedPosition = modelMatrix * Vec4f(hit.hitpoint, 1.0f);
                                transformedPosition /= transformedPosition.w;

                                hit.hitpoint = transformedPosition.GetXYZ();

                                hit.distance = (hit.hitpoint - ray.position).Length();

                                bvhResults.AddHit(hit);
                            }

                            outResults.Merge(std::move(bvhResults));

                            hasHit = true;
                        }

                        continue;
                    }
                    else
                    {
                        NodeLinkComponent* nodeLinkComponent = m_entityManager->TryGetComponent<NodeLinkComponent>(entry.value);
                        Handle<Node> node = nodeLinkComponent ? nodeLinkComponent->node.Lock() : nullptr;

                        HYP_LOG(Octree, Warning, "Entity #{} (node: {}) does not have a BVH component, using AABB instead", entry.value->Id(), node ? node->GetName() : NAME("<null>"));
                    }
                }
            }

            if (ray.TestAABB(entry.aabb, entry.value->Id().Value(), nullptr, aabbResult))
            {
                outResults.Merge(std::move(aabbResult));

                hasHit = true;
            }
        }

        if (m_isDivided)
        {
            for (const Octant& octant : m_octants)
            {
                Assert(octant.octree != nullptr);

                if (static_cast<Octree*>(octant.octree.Get())->TestRay(ray, outResults))
                {
                    hasHit = true;
                }
            }
        }
    }

    return hasHit;
}

} // namespace hyperion
