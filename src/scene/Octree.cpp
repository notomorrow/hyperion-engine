/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/Octree.hpp>
#include <scene/Entity.hpp>
#include <scene/Node.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/BVHComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/NodeLinkComponent.hpp>

#include <scene/camera/Camera.hpp>

#include <core/algorithm/Every.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

const BoundingBox Octree::default_bounds = BoundingBox({ -250.0f }, { 250.0f });

// 0x80 For index bit because we reserve the highest bit for invalid octants
// 0xff for depth because +1 (used for child octant id) will cause it to overflow to 0
OctantID OctantID::Invalid()
{
    return OctantID(OctantID::invalid_bits, 0xff);
}

void OctreeState::MarkOctantDirty(OctantID octant_id)
{
    const OctantID prev_state = rebuild_state;

    if (octant_id.IsInvalid())
    {
        return;
    }

    if (rebuild_state.IsInvalid())
    {
        rebuild_state = octant_id;

        return;
    }

    while (octant_id != rebuild_state && !octant_id.IsChildOf(rebuild_state) && !rebuild_state.IsInvalid())
    {
        rebuild_state = rebuild_state.GetParent();
    }

    // should always end up at root if it doesnt match any
    AssertThrow(rebuild_state != OctantID::Invalid());
}

Octree::Octree(const RC<EntityManager>& entity_manager)
    : Octree(entity_manager, default_bounds)
{
}

Octree::Octree(const RC<EntityManager>& entity_manager, const BoundingBox& aabb)
    : Octree(entity_manager, aabb, nullptr, 0)
{
    m_state = new OctreeState;
}

Octree::Octree(const RC<EntityManager>& entity_manager, const BoundingBox& aabb, Octree* parent, uint8 index)
    : m_entity_manager(entity_manager),
      m_aabb(aabb),
      m_parent(nullptr),
      m_is_divided(false),
      m_state(nullptr),
      m_octant_id(index, OctantID::Invalid()),
      m_invalidation_marker(0)
{
    if (parent != nullptr)
    {
        SetParent(parent); // call explicitly to set root ptr
    }

    AssertThrow(m_octant_id.GetIndex() == index);

    InitOctants();
}

Octree::~Octree()
{
    Clear();

    if (IsRoot())
    {
        delete m_state;
    }
}

void Octree::SetEntityManager(const RC<EntityManager>& entity_manager)
{
    HYP_SCOPE;

    m_entity_manager = entity_manager;

    if (IsDivided())
    {
        for (Octant& octant : m_octants)
        {
            AssertThrow(octant.octree != nullptr);

            octant.octree->SetEntityManager(m_entity_manager);
        }
    }
}

void Octree::SetParent(Octree* parent)
{
    HYP_SCOPE;

    m_parent = parent;

    if (m_parent != nullptr)
    {
        m_state = m_parent->m_state;
    }
    else
    {
        m_state = nullptr;
    }

    m_octant_id = OctantID(m_octant_id.GetIndex(), parent != nullptr ? parent->m_octant_id : OctantID::Invalid());

    if (IsDivided())
    {
        for (Octant& octant : m_octants)
        {
            AssertThrow(octant.octree != nullptr);

            octant.octree->SetParent(this);
        }
    }
}

bool Octree::EmptyDeep(int depth, uint8 octant_mask) const
{
    HYP_SCOPE;

    if (!Empty())
    {
        return false;
    }

    if (!m_is_divided)
    {
        return true;
    }

    if (depth != 0)
    {
        return Every(m_octants, [depth, octant_mask](const Octant& octant)
            {
                if (octant_mask & (1u << octant.octree->m_octant_id.GetIndex()))
                {
                    return octant.octree->EmptyDeep(depth - 1);
                }

                return true;
            });
    }

    return true;
}

void Octree::InitOctants()
{
    HYP_SCOPE;

    const Vec3f divided_aabb_dimensions = m_aabb.GetExtent() / 2.0f;

    for (uint32 x = 0; x < 2; x++)
    {
        for (uint32 y = 0; y < 2; y++)
        {
            for (uint32 z = 0; z < 2; z++)
            {
                const uint32 index = 4 * x + 2 * y + z;

                m_octants[index] = {
                    .aabb = BoundingBox(
                        m_aabb.GetMin() + divided_aabb_dimensions * Vec3f(float(x), float(y), float(z)),
                        m_aabb.GetMin() + divided_aabb_dimensions * (Vec3f(float(x), float(y), float(z)) + Vec3f(1.0f)))
                };
            }
        }
    }
}

Octree* Octree::GetChildOctant(OctantID octant_id)
{
    HYP_SCOPE;

    if (octant_id == OctantID::Invalid())
    {
#ifdef HYP_OCTREE_DEBUG
        HYP_LOG(Octree, Warning,
            "Invalid octant id {}:{}: Octant is invalid",
            octant_id.GetDepth(), octant_id.GetIndex());
#endif

        return nullptr;
    }

    if (octant_id == m_octant_id)
    {
        return this;
    }

    if (octant_id.depth <= m_octant_id.depth)
    {
#ifdef HYP_OCTREE_DEBUG
        HYP_LOG(Octree, Warning,
            "Octant id {}:{} is not a child of {}:{}: Octant is not deep enough",
            octant_id.GetDepth(), octant_id.GetIndex(),
            m_octant_id.GetDepth(), m_octant_id.GetIndex());
#endif

        return nullptr;
    }

    Octree* current = this;

    for (uint32 depth = m_octant_id.depth + 1; depth <= octant_id.depth; depth++)
    {
        const uint8 index = octant_id.GetIndex(depth);

        if (!current || !current->IsDivided())
        {
#ifdef HYP_OCTREE_DEBUG
            HYP_DEBUG(Octree, Warning,
                "Octant id {}:{} is not a child of {}:{}: Octant {}:{} is not divided",
                octant_id.GetDepth(), octant_id.GetIndex(),
                m_octant_id.GetDepth(), m_octant_id.GetIndex(),
                current ? current->m_octant_id.GetDepth() : ~0u,
                current ? current->m_octant_id.GetIndex() : ~0u);
#endif

            return nullptr;
        }

        current = current->m_octants[index].octree.Get();
    }

    return current;
}

void Octree::Divide()
{
    HYP_SCOPE;

    AssertThrow(!IsDivided());

    for (int i = 0; i < 8; i++)
    {
        Octant& octant = m_octants[i];
        AssertThrow(octant.octree == nullptr);

        octant.octree = MakeUnique<Octree>(m_entity_manager, octant.aabb, this, uint8(i));
    }

    m_is_divided = true;
}

void Octree::Undivide()
{
    HYP_SCOPE;

    AssertThrow(IsDivided());
    AssertThrowMsg(m_entries.Empty(), "Undivide() should be called on octrees with no remaining entries");

    for (Octant& octant : m_octants)
    {
        AssertThrow(octant.octree != nullptr);

        if (octant.octree->IsDivided())
        {
            octant.octree->Undivide();
        }

        octant.octree.Reset();
    }

    m_is_divided = false;
}

void Octree::Invalidate()
{
    HYP_SCOPE;

    m_invalidation_marker++;

    if (IsDivided())
    {
        for (Octant& octant : m_octants)
        {
            AssertThrow(octant.octree != nullptr);

            octant.octree->Invalidate();
        }
    }
}

void Octree::CollapseParents(bool allow_rebuild)
{
    HYP_SCOPE;

    m_state->MarkOctantDirty(m_octant_id);

    if (IsDivided() || !Empty())
    {
        return;
    }

    Octree *iteration = m_parent,
           *highest_empty = nullptr;

    while (iteration != nullptr && iteration->Empty())
    {
        for (const Octant& octant : iteration->m_octants)
        {
            AssertThrow(octant.octree != nullptr);

            if (octant.octree.Get() == highest_empty)
            {
                /* Do not perform a check on our entry, as we've already
                 * checked it by going up the chain.
                 * As `iter` becomes the parent of this entry we're currently working with,
                 * we will not have to perform duplicate EmptyDeep() checks on any octants because of this check. */
                continue;
            }

            if (!octant.octree->EmptyDeep())
            {
                goto undivide;
            }
        }

        highest_empty = iteration;
        iteration = iteration->m_parent;
    }

undivide:
    if (highest_empty != nullptr)
    {
        if (allow_rebuild)
        {
            highest_empty->Undivide();
        }
        else
        {
            m_state->MarkOctantDirty(highest_empty->GetOctantID());
        }
    }
}

void Octree::Clear()
{
    HYP_SCOPE;

    Array<Entry> entries;
    Clear_Internal(entries, /* undivide */ true);

    if (m_entity_manager)
    {
        AssertThrow(Threads::IsOnThread(m_entity_manager->GetOwnerThreadID()));

        for (Entry& entry : entries)
        {
            if (VisibilityStateComponent* visibility_state_component = m_entity_manager->TryGetComponent<VisibilityStateComponent>(entry.entity.GetID()))
            {
                visibility_state_component->octant_id = OctantID::Invalid();
                visibility_state_component->visibility_state = nullptr;
            }

            m_entity_manager->AddTag<EntityTag::UPDATE_VISIBILITY_STATE>(entry.entity.GetID());
        }
    }

    RebuildEntriesHash();
}

void Octree::Clear_Internal(Array<Entry>& out_entries, bool undivide)
{
    HYP_SCOPE;

    out_entries.Reserve(out_entries.Size() + m_entries.Size());

    for (Entry& entry : m_entries)
    {
        if (m_state != nullptr)
        {
            auto entity_to_octree_it = m_state->entity_to_octree.Find(entry.entity);

            AssertThrow(entity_to_octree_it != m_state->entity_to_octree.End());
            AssertThrow(entity_to_octree_it->second == this);

            m_state->entity_to_octree.Erase(entity_to_octree_it);
        }

        out_entries.PushBack(std::move(entry));
    }

    m_entries.Clear();

    if (!m_is_divided)
    {
        return;
    }

    for (Octant& octant : m_octants)
    {
        AssertThrow(octant.octree != nullptr);

        octant.octree->Clear_Internal(out_entries, /* undivide */ false);
    }

    if (undivide && IsDivided())
    {
        Undivide();
    }
}

Octree::InsertResult Octree::Insert(const WeakHandle<Entity>& entity, const BoundingBox& aabb, bool allow_rebuild)
{
    HYP_SCOPE;

    if (aabb.IsValid() && aabb.IsFinite())
    {
        if (allow_rebuild)
        {
            if (!m_aabb.Contains(aabb))
            {
                auto rebuild_result = RebuildExtend_Internal(aabb);

                if (!rebuild_result.first)
                {
#ifdef HYP_OCTREE_DEBUG
                    HYP_LOG(Octree, Warning,
                        "Failed to rebuild octree when inserting entity #{}",
                        id.Value());
#endif

                    return rebuild_result;
                }
            }
        }

        // stop recursing if we are at max depth
        if (m_octant_id.GetDepth() < OctantID::max_depth - 1)
        {
            for (Octant& octant : m_octants)
            {
                if (octant.aabb.Contains(aabb))
                {
                    if (!IsDivided())
                    {
                        if (allow_rebuild)
                        {
                            Divide();
                        }
                        else
                        {
                            // do not use this octant if it has not been divided yet.
                            // instead, we'll insert into the current octant and mark it dirty
                            // so it will get added after the fact.
                            continue;
                        }
                    }

                    AssertThrow(octant.octree != nullptr);

                    return octant.octree->Insert(entity, aabb, allow_rebuild);
                }
            }
        }
    }

    m_state->MarkOctantDirty(m_octant_id);

    return Insert_Internal(entity, aabb);
}

Octree::InsertResult Octree::Insert_Internal(const WeakHandle<Entity>& entity, const BoundingBox& aabb)
{
    HYP_SCOPE;

    m_entries.Set(Entry { entity, aabb });

    if (m_state != nullptr)
    {
        AssertThrowMsg(m_state->entity_to_octree.Find(entity) == m_state->entity_to_octree.End(), "Entity must not already be in octree hierarchy.");

        m_state->entity_to_octree.Insert(entity, this);

        // debug check
        AssertThrow(m_state->entity_to_octree.FindAs(entity.GetID())->second == this);
    }

    return {
        { Octree::Result::OCTREE_OK },
        m_octant_id
    };
}

Octree::Result Octree::Remove(ID<Entity> id, bool allow_rebuild)
{
    HYP_SCOPE;

    if (m_state != nullptr)
    {
        const auto it = m_state->entity_to_octree.FindAs(id);

        if (it == m_state->entity_to_octree.End())
        {
            HYP_BREAKPOINT;
            return { Result::OCTREE_ERR, "Not found in entry map" };
        }

        if (Octree* octant = it->second)
        {
            return octant->Remove_Internal(id, allow_rebuild);
        }

        return { Result::OCTREE_ERR, "Could not be removed from any sub octants" };
    }

    return Remove_Internal(id, allow_rebuild);
}

Octree::Result Octree::Remove_Internal(ID<Entity> id, bool allow_rebuild)
{
    HYP_SCOPE;

    const auto it = m_entries.FindAs(id);

    if (it == m_entries.End())
    {
        if (m_is_divided)
        {
            for (Octant& octant : m_octants)
            {
                AssertThrow(octant.octree != nullptr);

                if (Result octant_result = octant.octree->Remove_Internal(id, allow_rebuild))
                {
                    return octant_result;
                }
            }
        }

        return { Result::OCTREE_ERR, "Could not be removed from any sub octants and not found in this octant" };
    }

    if (m_state != nullptr)
    {
        auto entity_to_octree_it = m_state->entity_to_octree.FindAs(id);

        AssertThrow(entity_to_octree_it != m_state->entity_to_octree.End());
        AssertThrow(entity_to_octree_it->second == this);

        m_state->entity_to_octree.Erase(entity_to_octree_it);
    }

    m_entries.Erase(it);

    m_state->MarkOctantDirty(m_octant_id);

    if (!m_is_divided && m_entries.Empty())
    {
        Octree* last_empty_parent = nullptr;

        if (Octree* parent = m_parent)
        {
            const Octree* child = this;

            while (parent->EmptyDeep(DEPTH_SEARCH_INF, 0xff & ~(1 << child->m_octant_id.GetIndex())))
            { // do not search this branch of the tree again
                last_empty_parent = parent;

                if (parent->m_parent == nullptr)
                {
                    break;
                }

                child = parent;
                parent = child->m_parent;
            }
        }

        if (last_empty_parent != nullptr)
        {
            AssertThrow(last_empty_parent->EmptyDeep(DEPTH_SEARCH_INF));

            /* At highest empty parent octant, call Undivide() to collapse entries */
            if (allow_rebuild)
            {
                last_empty_parent->Undivide();
            }
            else
            {
                m_state->MarkOctantDirty(last_empty_parent->GetOctantID());
            }
        }
    }

    return {};
}

Octree::InsertResult Octree::Move(ID<Entity> id, const BoundingBox& aabb, bool allow_rebuild, EntrySet::Iterator it)
{
    HYP_SCOPE;

    const BoundingBox& new_aabb = aabb;

    const bool is_root = IsRoot();
    const bool contains = m_aabb.Contains(new_aabb);

    if (!contains)
    {
        // NO LONGER CONTAINS AABB

        if (is_root)
        {
            // have to rebuild, invalidating child octants.
            // which we have a Contains() check for child entries walking upwards

#ifdef HYP_OCTREE_DEBUG
            HYP_LOG(Octree, Debug,
                "In root, but does not contain entry aabb, so rebuilding octree. {}",
                id.Value());
#endif

            if (allow_rebuild)
            {
                return RebuildExtend_Internal(new_aabb);
            }
            else
            {
                m_state->MarkOctantDirty(m_octant_id);

                // Moved outside of the root octree, but we keep it here for now.
                // Next call of PerformUpdates(), we will extend the octree.
                return {
                    { Octree::Result::OCTREE_OK },
                    m_octant_id
                };
            }
        }

        // not root
#ifdef HYP_OCTREE_DEBUG
        HYP_LOG(Octree, Debug,
            "Moving entity #{} into the closest fitting (or root) parent",
            id.Value());
#endif

        Optional<InsertResult> parent_insert_result;

        /* Contains is false at this point */
        Octree* parent = m_parent;
        Octree* last_parent = m_parent;

        while (parent != nullptr)
        {
            last_parent = parent;

            if (parent->m_aabb.Contains(new_aabb))
            {
                if (it != m_entries.End())
                {
                    if (m_state != nullptr)
                    {
                        auto entity_to_octree_it = m_state->entity_to_octree.FindAs(id);

                        AssertThrow(entity_to_octree_it != m_state->entity_to_octree.End());
                        AssertThrow(entity_to_octree_it->second == this);

                        m_state->entity_to_octree.Erase(entity_to_octree_it);
                    }

                    m_entries.Erase(it);
                }

                parent_insert_result = parent->Move(id, aabb, allow_rebuild, parent->m_entries.End());

                break;
            }

            parent = parent->m_parent;
        }

        if (parent_insert_result.HasValue())
        { // succesfully inserted, safe to call CollapseParents()
            // Entry has now been added to it's appropriate octant which is a parent of this -
            // collapse this up to the top
            CollapseParents(allow_rebuild);

            return parent_insert_result.Get();
        }

        // not inserted because no Move() was called on parents (because they don't contain AABB),
        // have to _manually_ call Move() which will go to the above branch for the parent octant,
        // this invalidating `this`

#ifdef HYP_OCTREE_DEBUG
        HYP_LOG(Octree, Debug,
            "In child, no parents contain AABB so calling Move() on last valid octant (root). This will invalidate `this`.. {}",
            id.Value());
#endif

        AssertThrow(last_parent != nullptr);

        return last_parent->Move(id, aabb, allow_rebuild, last_parent->m_entries.End());
    }

    // CONTAINS AABB HERE

    if (allow_rebuild)
    {
        // Check if we can go deeper.
        for (Octant& octant : m_octants)
        {
            if (octant.aabb.Contains(new_aabb))
            {
                if (it != m_entries.End())
                {
                    if (m_state != nullptr)
                    {
                        auto entity_to_octree_it = m_state->entity_to_octree.FindAs(id);

                        AssertThrow(entity_to_octree_it != m_state->entity_to_octree.End());
                        AssertThrow(entity_to_octree_it->second == this);

                        m_state->entity_to_octree.Erase(entity_to_octree_it);
                    }

                    m_entries.Erase(it);
                }

                if (!IsDivided())
                {
                    if (allow_rebuild && m_octant_id.GetDepth() < OctantID::max_depth - 1)
                    {
                        Divide();
                    }
                    else
                    {
                        continue;
                    }
                }

                AssertThrow(octant.octree != nullptr);

                const auto octant_move_result = octant.octree->Move(id, aabb, allow_rebuild, octant.octree->m_entries.End());
                AssertThrow(octant_move_result.first);

                return octant_move_result;
            }
        }
    }
    else
    {
        m_state->MarkOctantDirty(m_octant_id);
    }

    if (it != m_entries.End())
    { /* Not moved out of this octant (for now) */
        it->aabb = new_aabb;
    }
    else
    { /* Moved into new octant */
        WeakHandle<Entity> entity { id };
        AssertThrow(entity.IsValid());

        m_entries.Insert(Entry { entity, new_aabb });

        if (m_state != nullptr)
        {
            AssertThrowMsg(m_state->entity_to_octree.Find(entity) == m_state->entity_to_octree.End(), "Entity must not already be in octree hierarchy.");

            m_state->entity_to_octree.Insert(entity, this);

            // debug check
            AssertThrow(m_state->entity_to_octree.FindAs(entity.GetID())->second == this);
        }

#ifdef HYP_OCTREE_DEBUG
        HYP_LOG(Octree, Debug,
            "Entity #{} octant_id was moved to {}:{}",
            id.Value(), m_octant_id.GetDepth(), m_octant_id.GetIndex());
#endif
    }

    return {
        { Octree::Result::OCTREE_OK },
        m_octant_id
    };
}

Octree::InsertResult Octree::Update(ID<Entity> id, const BoundingBox& aabb, bool force_invalidation, bool allow_rebuild)
{
    HYP_SCOPE;

    if (m_state != nullptr)
    {
        const auto it = m_state->entity_to_octree.FindAs(id);

        if (it == m_state->entity_to_octree.End())
        {
            return {
                { Result::OCTREE_ERR, "Object not found in entry map!" },
                OctantID::Invalid()
            };
        }

        if (Octree* octree = it->second)
        {
            return octree->Update_Internal(id, aabb, force_invalidation, allow_rebuild);
        }

        return {
            { Result::OCTREE_ERR, "Object has no octree in entry map!" },
            OctantID::Invalid()
        };
    }

    return Update_Internal(id, aabb, force_invalidation, allow_rebuild);
}

Octree::InsertResult Octree::Update_Internal(ID<Entity> id, const BoundingBox& aabb, bool force_invalidation, bool allow_rebuild)
{
    HYP_SCOPE;

    const auto it = m_entries.FindAs(id);

    if (it == m_entries.End())
    {
        if (m_is_divided)
        {
            for (Octant& octant : m_octants)
            {
                AssertThrow(octant.octree != nullptr);

                auto update_internal_result = octant.octree->Update_Internal(id, aabb, force_invalidation, allow_rebuild);

                if (update_internal_result.first)
                {
                    return update_internal_result;
                }
            }
        }

        return {
            { Result::OCTREE_ERR, "Could not update in any sub octants" },
            OctantID::Invalid()
        };
    }

    if (force_invalidation)
    {
        HYP_LOG(Octree, Debug,
            "Forcing invalidation of octant entity #{}",
            id.Value());

        // force invalidation of this entry so the octant's hash will be updated
        Invalidate();
    }

    const BoundingBox& new_aabb = aabb;
    const BoundingBox& old_aabb = it->aabb;

    if (new_aabb == old_aabb)
    {
        if (force_invalidation)
        {
            // force invalidation of this entry so the octant's hash will be updated
            m_state->MarkOctantDirty(m_octant_id);
        }

        /* AABB has not changed - no need to update */
        return {
            { Result::OCTREE_OK },
            m_octant_id
        };
    }

    /* AABB has changed to we remove it from this octree and either:
     * If we don't contain it anymore - insert it from the highest level octree that still contains the aabb and then walking down from there
     * If we do still contain it - we will remove it from this octree and re-insert it to find the deepest child octant
     */

    return Move(id, new_aabb, allow_rebuild, it);
}

Octree::InsertResult Octree::Rebuild()
{
    HYP_SCOPE;

#ifdef HYP_OCTREE_DEBUG
    HYP_LOG(Octree, Debug, "Rebuild octant (Index: {}, Depth: {})", m_octant_id.GetIndex(), m_octant_id.GetDepth());
#endif

    if (m_entity_manager)
    {
        AssertThrow(Threads::IsOnThread(m_entity_manager->GetOwnerThreadID()));
    }

    Array<Entry> new_entries;
    Clear_Internal(new_entries, /* undivide */ true);

    BoundingBox prev_aabb = m_aabb;

    if (IsRoot())
    {
        m_aabb = BoundingBox::Empty();
    }

    for (auto& it : new_entries)
    {
        if (it.aabb.IsValid() && it.aabb.IsFinite())
        {
            if (IsRoot())
            {
                m_aabb = m_aabb.Union(it.aabb);
            }
            else
            {
                AssertThrow(m_aabb.Contains(it.aabb));
            }
        }

        auto insert_result = Insert(it.entity, it.aabb, true /* allow rebuild */);

        if (!insert_result.first)
        {
            return insert_result;
        }

        if (m_entity_manager)
        {
            VisibilityStateComponent* visibility_state_component = m_entity_manager->TryGetComponent<VisibilityStateComponent>(it.entity.GetID());

            if (visibility_state_component)
            {
                visibility_state_component->octant_id = insert_result.second;
                visibility_state_component->visibility_state = nullptr;
            }
            else
            {
                m_entity_manager->AddComponent<VisibilityStateComponent>(it.entity.GetID(), VisibilityStateComponent { .octant_id = insert_result.second, .visibility_state = nullptr });
            }

            m_entity_manager->AddTag<EntityTag::UPDATE_VISIBILITY_STATE>(it.entity.GetID());
        }
    }

    return {
        { Octree::Result::OCTREE_OK },
        m_octant_id
    };
}

Octree::InsertResult Octree::Rebuild(const BoundingBox& new_aabb)
{
    HYP_SCOPE;

    if (m_entity_manager)
    {
        AssertThrow(Threads::IsOnThread(m_entity_manager->GetOwnerThreadID()));
    }

    Array<Entry> new_entries;
    Clear_Internal(new_entries, /* undivide */ true);

    m_aabb = new_aabb;

    for (auto& it : new_entries)
    {
        auto insert_result = Insert(it.entity, it.aabb, true /* allow rebuild */);

        if (!insert_result.first)
        {
            return insert_result;
        }

        if (m_entity_manager)
        {
            VisibilityStateComponent* visibility_state_component = m_entity_manager->TryGetComponent<VisibilityStateComponent>(it.entity.GetID());

            if (visibility_state_component)
            {
                visibility_state_component->octant_id = insert_result.second;
                visibility_state_component->visibility_state = nullptr;
            }
            else
            {
                m_entity_manager->AddComponent<VisibilityStateComponent>(it.entity.GetID(), VisibilityStateComponent { .octant_id = insert_result.second, .visibility_state = nullptr });
            }

            m_entity_manager->AddTag<EntityTag::UPDATE_VISIBILITY_STATE>(it.entity.GetID());
        }
    }

    return {
        { Octree::Result::OCTREE_OK },
        m_octant_id
    };
}

Octree::InsertResult Octree::RebuildExtend_Internal(const BoundingBox& extend_include_aabb)
{
    HYP_SCOPE;

    if (!extend_include_aabb.IsValid())
    {
        return {
            { Octree::Result::OCTREE_ERR, "AABB is in invalid state" },
            OctantID::Invalid()
        };
    }

    if (!extend_include_aabb.IsFinite())
    {
        return {
            { Octree::Result::OCTREE_ERR, "AABB is not finite" },
            OctantID::Invalid()
        };
    }

    // have to grow the aabb by rebuilding the octree
    BoundingBox new_aabb(m_aabb.Union(extend_include_aabb));
    // grow our new aabb by a predetermined growth factor,
    // to keep it from constantly resizing
    new_aabb *= growth_factor;

    return Rebuild(new_aabb);
}

void Octree::PerformUpdates()
{
    HYP_SCOPE;

    AssertThrow(m_state != nullptr);

    if (!m_state->NeedsRebuild())
    {
        // No octant to rebuild, skipping
        return;
    }

    Octree* octant = GetChildOctant(m_state->rebuild_state);
    AssertThrow(octant != nullptr);

    const auto rebuild_result = octant->Rebuild();

    RebuildEntriesHash();

    if (!rebuild_result.first)
    {
        HYP_LOG(Octree, Warning,
            "Failed to rebuild octree when performing updates: {}",
            rebuild_result.first.message);
    }
    else
    {
        // set rebuild state back to invalid if rebuild was successful
        m_state->rebuild_state = OctantID::Invalid();
    }
}

void Octree::CollectEntries(Array<const Entry*>& out_entries) const
{
    HYP_SCOPE;

    out_entries.Reserve(out_entries.Size() + m_entries.Size());

    for (const Octree::Entry& entry : m_entries)
    {
        out_entries.PushBack(&entry);
    }

    if (m_is_divided)
    {
        for (const Octant& octant : m_octants)
        {
            AssertThrow(octant.octree != nullptr);

            octant.octree->CollectEntries(out_entries);
        }
    }
}

void Octree::CollectEntriesInRange(const Vec3f& position, float radius, Array<const Entry*>& out_entries) const
{
    HYP_SCOPE;

    const BoundingBox inclusion_aabb(position - radius, position + radius);

    if (!inclusion_aabb.Overlaps(m_aabb))
    {
        return;
    }

    out_entries.Reserve(out_entries.Size() + m_entries.Size());

    for (const Octree::Entry& entry : m_entries)
    {
        if (inclusion_aabb.Overlaps(entry.aabb))
        {
            out_entries.PushBack(&entry);
        }
    }

    if (m_is_divided)
    {
        for (const Octant& octant : m_octants)
        {
            AssertThrow(octant.octree != nullptr);

            octant.octree->CollectEntriesInRange(position, radius, out_entries);
        }
    }
}

bool Octree::GetNearestOctants(const Vec3f& position, FixedArray<Octree*, 8>& out) const
{
    HYP_SCOPE;

    if (!m_aabb.ContainsPoint(position))
    {
        return false;
    }

    if (!m_is_divided)
    {
        return false;
    }

    for (const Octant& octant : m_octants)
    {
        AssertThrow(octant.octree != nullptr);

        if (octant.octree->GetNearestOctants(position, out))
        {
            return true;
        }
    }

    for (SizeType i = 0; i < m_octants.Size(); i++)
    {
        out[i] = m_octants[i].octree.Get();
    }

    return true;
}

bool Octree::GetNearestOctant(const Vec3f& position, Octree const*& out) const
{
    HYP_SCOPE;

    if (!m_aabb.ContainsPoint(position))
    {
        return false;
    }

    if (m_is_divided)
    {
        for (const Octant& octant : m_octants)
        {
            AssertThrow(octant.octree != nullptr);

            if (octant.octree->GetNearestOctant(position, out))
            {
                return true;
            }
        }
    }

    out = this;

    return true;
}

bool Octree::GetFittingOctant(const BoundingBox& aabb, Octree const*& out) const
{
    if (!m_aabb.Contains(aabb))
    {
        out = nullptr;

        return false;
    }

    if (m_is_divided)
    {
        for (const Octant& octant : m_octants)
        {
            AssertThrow(octant.octree != nullptr);

            if (octant.octree->GetFittingOctant(aabb, out))
            {
                return true;
            }
        }
    }

    out = this;

    return true;
}

void Octree::NextVisibilityState()
{
    HYP_SCOPE;

    AssertThrow(IsRoot());

    m_visibility_state.Next();
}

void Octree::CalculateVisibility(const Handle<Camera>& camera)
{
    HYP_SCOPE;

    AssertThrow(IsRoot());

    UpdateVisibilityState(camera, m_visibility_state.validity_marker);
}

void Octree::UpdateVisibilityState(const Handle<Camera>& camera, uint16 validity_marker)
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
    uint8 child_index = uint8(-1);

    while (true)
    {
        // Process current node.
        current->m_visibility_state.validity_marker = validity_marker;
        current->m_visibility_state.MarkAsValid(camera.GetID());

        if (current->m_is_divided)
        {
            bool descended = false;

            for (uint8 i = child_index + 1; i < 8; i++)
            {
                if (!frustum.ContainsAABB(current->m_octants[i].aabb))
                {
                    continue;
                }

                current = current->m_octants[i].octree.Get();
                child_index = uint8(-1);

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

        child_index = current->m_octant_id.GetIndex();
        current = current->m_parent;
    }
}

void Octree::ResetEntriesHash()
{
    HYP_SCOPE;

    m_entry_hashes = {};
}

void Octree::RebuildEntriesHash(uint32 level)
{
    HYP_SCOPE;

    ResetEntriesHash();

    for (const Entry& entry : m_entries)
    {
        const HashCode entry_hash_code = entry.GetHashCode();
        m_entry_hashes[0].Add(entry_hash_code);

        Array<EntityTag> tags;

        if (m_entity_manager)
        {
            // @FIXME: Having issue here where entity is no longer part of this entitymanager.
            // Must be getting moved to a different one.
            tags = m_entity_manager->GetTags(entry.entity.GetID());
        }

        for (uint32 i = 0; i < uint32(tags.Size()); i++)
        {
            const uint32 num_combinations = (1u << i);

            for (uint32 k = 0; k < num_combinations; k++)
            {
                uint32 mask = (1u << (uint32(tags[i]) - 1));

                for (uint32 j = 0; j < i; j++)
                {
                    if ((k & (1u << j)) != (1u << j))
                    {
                        continue;
                    }

                    mask |= (1u << (uint32(tags[j]) - 1));
                }

                AssertThrow(m_entry_hashes.Size() > mask);

                m_entry_hashes[mask].Add(entry_hash_code);
            }
        }
    }

    if (m_is_divided)
    {
        for (const Octant& octant : m_octants)
        {
            AssertThrow(octant.octree != nullptr);

            octant.octree->RebuildEntriesHash(level + 1);
        }
    }

    // Update parent's hash to include this octant's hash
    if (m_parent)
    {
        for (uint32 i = 0; i < uint32(m_entry_hashes.Size()); i++)
        {
            m_parent->m_entry_hashes[i].Add(m_entry_hashes[i]);
        }
    }
}

bool Octree::TestRay(const Ray& ray, RayTestResults& out_results, bool use_bvh) const
{
    HYP_SCOPE;

    bool has_hit = false;

    if (ray.TestAABB(m_aabb))
    {
        for (const Octree::Entry& entry : m_entries)
        {
            RayTestResults aabb_result;

            if (use_bvh)
            {
                if (m_entity_manager && entry.entity.IsValid())
                {
                    if (!m_entity_manager->HasEntity(entry.entity.GetID()))
                    {
                        continue;
                    }

                    // If the entity has a BVH associated with it, use that instead of the AABB for more accuracy
                    if (BVHComponent* bvh_component = m_entity_manager->TryGetComponent<BVHComponent>(entry.entity.GetID()))
                    {
                        Matrix4 model_matrix = Matrix4::Identity();
                        Matrix4 normal_matrix = Matrix4::Identity();

                        Ray local_space_ray = ray;

                        if (TransformComponent* transform_component = m_entity_manager->TryGetComponent<TransformComponent>(entry.entity.GetID()))
                        {
                            model_matrix = transform_component->transform.GetMatrix();
                            normal_matrix = model_matrix.Transposed().Inverted();

                            local_space_ray = model_matrix.Inverted() * ray;
                        }

                        RayTestResults local_bvh_results = bvh_component->bvh.TestRay(local_space_ray);

                        if (local_bvh_results.Any())
                        {
                            RayTestResults bvh_results;

                            for (RayHit hit : local_bvh_results)
                            {
                                hit.id = entry.entity.GetID().Value();
                                hit.user_data = nullptr;

                                Vec4f transformed_normal = normal_matrix * Vec4f(hit.normal, 0.0f);
                                hit.normal = transformed_normal.GetXYZ().Normalized();

                                Vec4f transformed_position = model_matrix * Vec4f(hit.hitpoint, 1.0f);
                                transformed_position /= transformed_position.w;

                                hit.hitpoint = transformed_position.GetXYZ();

                                hit.distance = (hit.hitpoint - ray.position).Length();

                                bvh_results.AddHit(hit);
                            }

                            out_results.Merge(std::move(bvh_results));

                            has_hit = true;
                        }

                        continue;
                    }
                    else
                    {
                        NodeLinkComponent* node_link_component = m_entity_manager->TryGetComponent<NodeLinkComponent>(entry.entity.GetID());
                        Handle<Node> node = node_link_component ? node_link_component->node.Lock() : nullptr;

                        HYP_LOG(Octree, Warning,
                            "Entity #{} (node: {}) does not have a BVH component, using AABB instead",
                            entry.entity.GetID().Value(),
                            node ? node->GetName() : "<null>");
                    }
                }
            }

            if (ray.TestAABB(entry.aabb, entry.entity.GetID().Value(), nullptr, aabb_result))
            {
                out_results.Merge(std::move(aabb_result));

                has_hit = true;
            }
        }

        if (m_is_divided)
        {
            for (const Octant& octant : m_octants)
            {
                AssertThrow(octant.octree != nullptr);

                if (octant.octree->TestRay(ray, out_results))
                {
                    has_hit = true;
                }
            }
        }
    }

    return has_hit;
}

} // namespace hyperion