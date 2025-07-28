/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <util/octree/Octree.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/FixedArray.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/HashSet.hpp>

#include <core/utilities/Pair.hpp>

#include <core/Handle.hpp>

#include <scene/Entity.hpp>
#include <scene/VisibilityState.hpp>

#include <scene/EntityTag.hpp>

#include <core/math/Vector3.hpp>
#include <core/math/BoundingBox.hpp>
#include <core/math/Ray.hpp>

#include <Types.hpp>

// #define HYP_OCTREE_DEBUG

namespace hyperion {

class Entity;
class EntityManager;
class Camera;

class Octree;

class HYP_API Octree final : public OctreeBase<Octree, Entity*>
{
public:
    Octree(const Handle<EntityManager>& entityManager);
    Octree(const Handle<EntityManager>& entityManager, const BoundingBox& aabb);
    Octree(const Handle<EntityManager>& entityManager, const BoundingBox& aabb, Octree* parent, uint8 index);

    virtual ~Octree() override;

    VisibilityState& GetVisibilityState()
    {
        return m_visibilityState;
    }

    const VisibilityState& GetVisibilityState() const
    {
        return m_visibilityState;
    }

    /*! \brief Get the EntityManager the Octree is using to manage entities.
     *  \returns The EntityManager the Octree is set to use */
    const Handle<EntityManager>& GetEntityManager() const
    {
        return m_entityManager;
    }

    /*! \brief Set the EntityManager for the Octree to use. For internal use from \ref{Scene} only
     *  \internal */
    void SetEntityManager(const Handle<EntityManager>& entityManager);

    /*! \brief Get a hashcode of all entities currently in this Octant that have the given tags (child octants affect this too)
     */
    template <EntityTag Tag>
    HYP_FORCE_INLINE HashCode GetEntryListHash() const
    {
        static_assert((uint32(Tag) < uint32(EntityTag::SAVABLE_MAX)), "All tags must have a value < EntityTag::SAVABLE_MAX");

        return HashCode(m_entryHashes[uint32(Tag)])
            .Add(m_invalidationMarker);
    }

    /*! \brief Get a hashcode of all entities currently in this Octant that match the mask tag (child octants affect this too)
     */
    HYP_FORCE_INLINE HashCode GetEntryListHash(EntityTag entityTag) const
    {
        Assert(uint32(entityTag) < m_entryHashes.Size());

        return HashCode(m_entryHashes[uint32(entityTag)])
            .Add(m_invalidationMarker);
    }

    HYP_FORCE_INLINE Octree* GetChildOctant(OctantId octantId)
    {
        return static_cast<Octree*>(OctreeBase::GetChildOctant(octantId));
    }

    void NextVisibilityState();
    void CalculateVisibility(const Handle<Camera>& camera);

    bool TestRay(const Ray& ray, RayTestResults& outResults, bool useBvh = true) const;

    virtual void Clear() override;
    virtual InsertResult Rebuild(const BoundingBox& newAabb, bool allowGrow) override;
    virtual void PerformUpdates() override;

private:
    static constexpr uint32 numEntryHashes = uint32(EntityTag::SAVABLE_MAX);

    virtual UniquePtr<Octree> CreateChildOctant(const BoundingBox& aabb, Octree* parent, uint8 index) override
    {
        return MakeUnique<Octree>(m_entityManager, aabb, this, index);
    }

    void ResetEntriesHash();
    void RebuildEntriesHash(uint32 level = 0);

    void UpdateVisibilityState(const Handle<Camera>& camera, uint16 validityMarker);

    Handle<EntityManager> m_entityManager;

    FixedArray<HashCode, numEntryHashes> m_entryHashes;

    VisibilityState m_visibilityState;
};

} // namespace hyperion
