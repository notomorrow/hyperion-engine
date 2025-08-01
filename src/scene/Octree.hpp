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

struct SceneOctreePayload
{
    struct Entry
    {
        Entity* value;
        BoundingBox aabb;

        Entry() = default;

        Entry(Entity* value, const BoundingBox& aabb)
            : value(value),
              aabb(aabb)
        {
        }

        Entry(const Entry& other)
            : value(other.value),
              aabb(other.aabb)
        {
        }

        Entry& operator=(const Entry& other)
        {
            value = other.value;
            aabb = other.aabb;

            return *this;
        }

        Entry(Entry&& other) noexcept
            : value(std::move(other.value)),
              aabb(other.aabb)
        {
            other.aabb = BoundingBox::Empty();
        }

        Entry& operator=(Entry&& other) noexcept
        {
            if (this == &other)
            {
                return *this;
            }

            value = std::move(other.value);
            aabb = other.aabb;

            other.aabb = BoundingBox::Empty();

            return *this;
        }

        ~Entry() = default;

        HYP_FORCE_INLINE bool operator==(const Entry& other) const
        {
            return value == other.value
                && aabb == other.aabb;
        }

        HYP_FORCE_INLINE bool operator!=(const Entry& other) const
        {
            return value != other.value
                || aabb != other.aabb;
        }

        HashCode GetHashCode() const
        {
            HashCode hc;

            hc.Add(HashCode::GetHashCode(value));
            hc.Add(aabb.GetHashCode());

            return hc;
        }
    };

    using EntrySet = HashSet<Entry, &Entry::value>;

    EntrySet entries;

    HYP_FORCE_INLINE bool Empty() const
    {
        return entries.Empty();
    }
};

class HYP_API Octree final : public OctreeBase<Octree, SceneOctreePayload>
{
public:
    Octree(const Handle<EntityManager>& entityManager);
    Octree(const Handle<EntityManager>& entityManager, const BoundingBox& aabb);
    Octree(const Handle<EntityManager>& entityManager, const BoundingBox& aabb, Octree* parent, uint8 index);

    virtual ~Octree() override;

    HYP_FORCE_INLINE const SceneOctreePayload::EntrySet& GetEntries() const
    {
        return m_payload.entries;
    }

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

    void Collect(Array<Entity*>& outEntities) const;
    void Collect(const BoundingSphere& bounds, Array<Entity*>& outEntities) const;
    void Collect(const BoundingBox& bounds, Array<Entity*>& outEntities) const;

    virtual void Clear() override;
    virtual Result Rebuild(const BoundingBox& newAabb, bool allowGrow) override;
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
