template <class Derived, class TEntry>
const BoundingBox OctreeBase<Derived, TEntry>::defaultBounds = BoundingBox({ -250.0f }, { 250.0f });

template <class Derived, class TEntry>
void OctreeState<Derived, TEntry>::MarkOctantDirty(OctantId octantId, bool needsRebuild)
{
    const DirtyState prevDirtyState = dirtyState;

    if (octantId.IsInvalid())
    {
        return;
    }

    if (dirtyState.octantId == OctantId::Invalid())
    {
        dirtyState = { octantId, needsRebuild };

        return;
    }

    while (octantId != dirtyState.octantId && !octantId.IsChildOf(dirtyState.octantId) && !dirtyState.octantId.IsInvalid())
    {
        octantId = dirtyState.octantId.GetParent();
    }

    // should always end up at root if it doesnt match any
    Assert(dirtyState.octantId != OctantId::Invalid());
    dirtyState.needsRebuild |= needsRebuild;
}

template <class Derived, class TEntry>
OctreeBase<Derived, TEntry>::OctreeBase(EnumFlags<OctreeFlags> flags)
    : OctreeBase(defaultBounds)
{
    m_flags = flags;
}

template <class Derived, class TEntry>
OctreeBase<Derived, TEntry>::OctreeBase(const BoundingBox& aabb)
    : OctreeBase(aabb, nullptr, 0)
{
    m_state = new OctreeState<Derived, TEntry>();
}

template <class Derived, class TEntry>
OctreeBase<Derived, TEntry>::OctreeBase(const BoundingBox& aabb, OctreeBase* parent, uint8 index)
    : m_aabb(aabb),
      m_parent(nullptr),
      m_isDivided(false),
      m_state(nullptr),
      m_octantId(index, OctantId::Invalid()),
      m_invalidationMarker(0),
      m_flags(OctreeFlags::OF_NONE)
{
    if (parent != nullptr)
    {
        SetParent(parent); // call explicitly to set root ptr
    }

    Assert(m_octantId.GetIndex() == index);

    InitOctants();
}

template <class Derived, class TEntry>
OctreeBase<Derived, TEntry>::~OctreeBase()
{
    if (IsRoot())
    {
        delete m_state;
    }
}

template <class Derived, class TEntry>
void OctreeBase<Derived, TEntry>::SetParent(OctreeBase* parent)
{
    m_parent = parent;

    if (m_parent != nullptr)
    {
        m_flags = parent->m_flags;
        m_state = m_parent->m_state;
    }
    else
    {
        m_state = nullptr;
    }

    m_octantId = OctantId(m_octantId.GetIndex(), parent != nullptr ? parent->m_octantId : OctantId::Invalid());

    if (IsDivided())
    {
        for (Octant& octant : m_octants)
        {
            Assert(octant.octree != nullptr);

            octant.octree->SetParent(this);
        }
    }
}

template <class Derived, class TEntry>
bool OctreeBase<Derived, TEntry>::EmptyDeep(int depth, uint8 octantMask) const
{
    if (!Empty())
    {
        return false;
    }

    if (!m_isDivided)
    {
        return true;
    }

    if (depth != 0)
    {
        return Every(m_octants, [depth, octantMask](const Octant& octant)
            {
                if (octantMask & (1u << octant.octree->m_octantId.GetIndex()))
                {
                    return octant.octree->EmptyDeep(depth - 1);
                }

                return true;
            });
    }

    return true;
}

template <class Derived, class TEntry>
void OctreeBase<Derived, TEntry>::InitOctants()
{
    const Vec3f dividedAabbDimensions = m_aabb.GetExtent() / 2.0f;

    for (uint32 x = 0; x < 2; x++)
    {
        for (uint32 y = 0; y < 2; y++)
        {
            for (uint32 z = 0; z < 2; z++)
            {
                const uint32 index = 4 * x + 2 * y + z;

                m_octants[index] = {
                    .aabb = BoundingBox(
                        m_aabb.GetMin() + dividedAabbDimensions * Vec3f(float(x), float(y), float(z)),
                        m_aabb.GetMin() + dividedAabbDimensions * (Vec3f(float(x), float(y), float(z)) + Vec3f(1.0f)))
                };
            }
        }
    }
}

template <class Derived, class TEntry>
OctreeBase<Derived, TEntry>* OctreeBase<Derived, TEntry>::GetChildOctant(OctantId octantId)
{
    if (octantId == OctantId::Invalid())
    {
        return nullptr;
    }

    if (octantId == m_octantId)
    {
        return this;
    }

    if (octantId.depth <= m_octantId.depth)
    {
        return nullptr;
    }

    OctreeBase* current = this;

    for (uint32 depth = m_octantId.depth + 1; depth <= octantId.depth; depth++)
    {
        const uint8 index = octantId.GetIndex(depth);

        if (!current || !current->IsDivided())
        {
#ifdef HYP_OCTREE_DEBUG
            HYP_DEBUG(OctreeBase, Warning,
                "Octant id {}:{} is not a child of {}:{}: Octant {}:{} is not divided",
                octantId.GetDepth(), octantId.GetIndex(),
                m_octantId.GetDepth(), m_octantId.GetIndex(),
                current ? current->m_octantId.GetDepth() : ~0u,
                current ? current->m_octantId.GetIndex() : ~0u);
#endif

            return nullptr;
        }

        current = current->m_octants[index].octree.Get();
    }

    return current;
}

template <class Derived, class TEntry>
void OctreeBase<Derived, TEntry>::Divide()
{
    Assert(!IsDivided());

    Array<Entry> entries;
    if (m_flags & OctreeFlags::OF_ONLY_INSERT_INTO_LEAF_NODES)
    {
        Clear(entries, false);
    }

    for (int i = 0; i < 8; i++)
    {
        Octant& octant = m_octants[i];
        Assert(octant.octree == nullptr);

        octant.octree = CreateChildOctant(octant.aabb, static_cast<Derived*>(this), uint8(i));
    }

    m_isDivided = true;

    // will only have elements if OF_ONLY_INSERT_INTO_LEAF_NODES is set
    // we do this to re-insert the elements into newly created child octants since we can't host them in our octant anymore
    if (entries.Any())
    {
        for (const Entry& entry : entries)
        {
            const BoundingBox& aabb = entry.aabb;

            auto insertResult = Insert(entry.value, aabb, /* allowRebuild */ true);
        }
    }

#ifdef HYP_DEBUG_MODE
    if (m_flags & OctreeFlags::OF_ONLY_INSERT_INTO_LEAF_NODES)
    {
        Assert(!IsDivided() || m_entries.Empty(),
            "OctreeBase::Divide() should not be called on octrees with entries if OF_ONLY_INSERT_INTO_LEAF_NODES is set");
    }
#endif
}

template <class Derived, class TEntry>
void OctreeBase<Derived, TEntry>::Undivide()
{
    Assert(IsDivided());
    Assert(m_entries.Empty(), "Undivide() should be called on octrees with no remaining entries");

    for (Octant& octant : m_octants)
    {
        Assert(octant.octree != nullptr);

        if (octant.octree->IsDivided())
        {
            octant.octree->Undivide();
        }

        octant.octree.Reset();
    }

    m_isDivided = false;
}

template <class Derived, class TEntry>
void OctreeBase<Derived, TEntry>::Invalidate()
{
    m_invalidationMarker++;

    if (IsDivided())
    {
        for (Octant& octant : m_octants)
        {
            Assert(octant.octree != nullptr);

            octant.octree->Invalidate();
        }
    }
}

template <class Derived, class TEntry>
void OctreeBase<Derived, TEntry>::CollapseParents(bool allowRebuild)
{
    if (IsDivided() || !Empty())
    {
        return;
    }

    OctreeBase *iteration = m_parent,
               *highestEmpty = nullptr;

    while (iteration != nullptr && iteration->Empty())
    {
        for (const Octant& octant : iteration->m_octants)
        {
            Assert(octant.octree != nullptr);

            if (octant.octree.Get() == highestEmpty)
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

        highestEmpty = iteration;
        iteration = iteration->m_parent;
    }

undivide:
    if (highestEmpty != nullptr)
    {
        if (allowRebuild)
        {
            highestEmpty->Undivide();
        }
        else
        {
            m_state->MarkOctantDirty(highestEmpty->GetOctantID(), true);
        }
    }
}

template <class Derived, class TEntry>
void OctreeBase<Derived, TEntry>::Clear()
{
    Array<Entry> entries;
    Clear(entries, /* undivide */ true);
}

template <class Derived, class TEntry>
void OctreeBase<Derived, TEntry>::Clear(Array<Entry>& outEntries, bool undivide)
{
    outEntries.Reserve(outEntries.Size() + m_entries.Size());

    for (Entry& entry : m_entries)
    {
        if (m_state != nullptr)
        {
            auto entryToOctreeIt = m_state->entryToOctree.Find(entry.value);

            Assert(entryToOctreeIt != m_state->entryToOctree.End());
            Assert(entryToOctreeIt->second == this);

            m_state->entryToOctree.Erase(entryToOctreeIt);
        }

        outEntries.PushBack(std::move(entry));
    }

    m_entries.Clear();

    if (!m_isDivided)
    {
        return;
    }

    for (Octant& octant : m_octants)
    {
        Assert(octant.octree != nullptr);

        octant.octree->Clear(outEntries, /* undivide */ false);
    }

    if (undivide && IsDivided())
    {
        Undivide();
    }
}

template <class Derived, class TEntry>
void OctreeBase<Derived, TEntry>::Clear(Array<TEntry>& outEntries, bool undivide)
{
    outEntries.Reserve(outEntries.Size() + m_entries.Size());

    for (Entry& entry : m_entries)
    {
        if (m_state != nullptr)
        {
            auto entryToOctreeIt = m_state->entryToOctree.Find(entry.value);

            Assert(entryToOctreeIt != m_state->entryToOctree.End());
            Assert(entryToOctreeIt->second == this);

            m_state->entryToOctree.Erase(entryToOctreeIt);
        }

        outEntries.PushBack(std::move(entry.value));
    }

    m_entries.Clear();

    if (!m_isDivided)
    {
        return;
    }

    for (Octant& octant : m_octants)
    {
        Assert(octant.octree != nullptr);

        octant.octree->Clear(outEntries, /* undivide */ false);
    }

    if (undivide && IsDivided())
    {
        Undivide();
    }
}

template <class Derived, class TEntry>
OctreeBase<Derived, TEntry>::InsertResult OctreeBase<Derived, TEntry>::Insert(const TEntry& value, const BoundingBox& aabb, bool allowRebuild)
{
    if (aabb.IsValid() && aabb.IsFinite())
    {
        if (m_aabb.Contains(aabb))
        {
            
            // stop recursing if we are at max depth
            if (m_octantId.GetDepth() < OctantId::maxDepth - 1)
            {
                for (Octant& octant : m_octants)
                {
                    if (!octant.aabb.Contains(aabb))
                    {
                        continue;
                    }
                    
                    if (!IsDivided())
                    {
                        if (!allowRebuild)
                        {
                            // do not use this octant if it has not been divided yet.
                            // instead, we'll insert into the THIS octant, marking it as dirty,
                            // so it will get added to the correct octant on Rebuild().
                            
                            m_state->MarkOctantDirty(m_octantId, true);
                            
                            break;
                        }
                        
                        Divide();
                    }
                    
                    Assert(octant.octree != nullptr);
                    
                    return octant.octree->Insert(value, aabb, allowRebuild);
                }
            }
        }
        else if (IsRoot() && allowRebuild)
        {
            auto rebuildResult = RebuildExtend_Internal(aabb);

            if (!rebuildResult.first)
            {
                return rebuildResult;
            }
        }
    }

    m_state->MarkOctantDirty(m_octantId);

    if ((m_flags & OctreeFlags::OF_ONLY_INSERT_INTO_LEAF_NODES) && IsDivided())
    {
        // we'll allow inserting infinite AABBs into the root node, but only if OF_ONLY_INSERT_INTO_LEAF_NODES is /not/ set.
        // otherwise it'll return an error.
        if (!aabb.IsValid() || !aabb.IsFinite())
        {
            return { { Result::OCTREE_ERR, "Invalid AABB for insertion" }, m_octantId };
        }

        // we can't insert into a divided octree if we only want to insert into leaf nodes
        // we need to find the best fit leaf node instead
        OctreeBase* bestFit = nullptr;

        for (Octant& octant : m_octants)
        {
            Assert(octant.octree != nullptr);

            if (!bestFit || (octant.aabb.GetCenter() - aabb.GetCenter()).LengthSquared() < (bestFit->GetAABB().GetCenter() - aabb.GetCenter()).LengthSquared())
            {
                bestFit = octant.octree.Get();
            }
        }

        Assert(bestFit != nullptr);

        return bestFit->Insert(value, aabb, allowRebuild);
    }

    return Insert_Internal(value, aabb);
}

template <class Derived, class TEntry>
OctreeBase<Derived, TEntry>::InsertResult OctreeBase<Derived, TEntry>::Insert_Internal(const TEntry& value, const BoundingBox& aabb)
{
    if (m_flags & OctreeFlags::OF_ONLY_INSERT_INTO_LEAF_NODES)
    {
        AssertDebug(!m_isDivided);
    }

    if (m_state != nullptr)
    {
        /*if (m_state->entryToOctree.Find(value) != m_state->entryToOctree.End())
        {
            return { { Result::OCTREE_ERR, "Entry already exists in entry map" }, m_octantId };
        }*/

        m_state->entryToOctree.Set(value, this);
    }

    m_entries.Set(Entry { value, aabb });

    return {
        { OctreeBase<Derived, TEntry>::Result::OCTREE_OK },
        m_octantId
    };
}

template <class Derived, class TEntry>
OctreeBase<Derived, TEntry>::Result OctreeBase<Derived, TEntry>::Remove(const TEntry& value, bool allowRebuild)
{
    if (m_state != nullptr)
    {
        const auto it = m_state->entryToOctree.Find(value);

        if (it == m_state->entryToOctree.End())
        {
            return { Result::OCTREE_ERR, "Not found in entry map" };
        }

        if (OctreeBase* octant = it->second)
        {
            return octant->Remove_Internal(value, allowRebuild);
        }

        return { Result::OCTREE_ERR, "Could not be removed from any sub octants" };
    }

    return Remove_Internal(value, allowRebuild);
}

template <class Derived, class TEntry>
OctreeBase<Derived, TEntry>::Result OctreeBase<Derived, TEntry>::Remove_Internal(const TEntry& value, bool allowRebuild)
{
    const auto it = m_entries.Find(value);

    if (it == m_entries.End())
    {
        if (m_isDivided)
        {
            for (Octant& octant : m_octants)
            {
                Assert(octant.octree != nullptr);

                if (Result octantResult = octant.octree->Remove_Internal(value, allowRebuild))
                {
                    return octantResult;
                }
            }
        }

        return { Result::OCTREE_ERR, "Could not be removed from any sub octants and not found in this octant" };
    }

    if (m_state != nullptr)
    {
        auto entryToOctreeIt = m_state->entryToOctree.Find(value);

        Assert(entryToOctreeIt != m_state->entryToOctree.End());
        Assert(entryToOctreeIt->second == this);

        m_state->entryToOctree.Erase(entryToOctreeIt);
    }

    m_entries.Erase(it);

    m_state->MarkOctantDirty(m_octantId);

    if (!m_isDivided && m_entries.Empty())
    {
        OctreeBase* lastEmptyParent = nullptr;

        if (OctreeBase* parent = m_parent)
        {
            const OctreeBase* child = this;

            while (parent->EmptyDeep(DEPTH_SEARCH_INF, 0xff & ~(1 << child->m_octantId.GetIndex())))
            { // do not search this branch of the tree again
                lastEmptyParent = parent;

                if (parent->m_parent == nullptr)
                {
                    break;
                }

                child = parent;
                parent = child->m_parent;
            }
        }

        if (lastEmptyParent != nullptr)
        {
            Assert(lastEmptyParent->EmptyDeep(DEPTH_SEARCH_INF));

            /* At highest empty parent octant, call Undivide() to collapse entries */
            if (allowRebuild)
            {
                lastEmptyParent->Undivide();
            }
            else
            {
                m_state->MarkOctantDirty(lastEmptyParent->GetOctantID(), true);
            }
        }
    }

    return {};
}

template <class Derived, class TEntry>
OctreeBase<Derived, TEntry>::InsertResult OctreeBase<Derived, TEntry>::Move(const TEntry& value, const BoundingBox& aabb, bool allowRebuild, EntrySet::Iterator it)
{
    const BoundingBox& newAabb = aabb;

    const bool isRoot = IsRoot();
    const bool contains = m_aabb.Contains(newAabb);

    if (!contains)
    {
        // NO LONGER CONTAINS AABB

        if (isRoot)
        {
            // have to rebuild, invalidating child octants.
            // which we have a Contains() check for child entries walking upwards

            if (allowRebuild)
            {
                return RebuildExtend_Internal(newAabb);
            }
            else
            {
                m_state->MarkOctantDirty(m_octantId, true);

                // Moved outside of the root octree, but we keep it here for now.
                // Next call of PerformUpdates(), we will extend the octree.
                return {
                    { OctreeBase<Derived, TEntry>::Result::OCTREE_OK },
                    m_octantId
                };
            }
        }

        // not root

        Optional<InsertResult> parentInsertResult;

        /* Contains is false at this point */
        OctreeBase* parent = m_parent;
        OctreeBase* lastParent = m_parent;

        while (parent != nullptr)
        {
            lastParent = parent;

            if (parent->m_aabb.Contains(newAabb))
            {
                if (it != m_entries.End())
                {
                    if (m_state != nullptr)
                    {
                        auto entryToOctreeIt = m_state->entryToOctree.Find(value);

                        Assert(entryToOctreeIt != m_state->entryToOctree.End());
                        Assert(entryToOctreeIt->second == this);

                        m_state->entryToOctree.Erase(entryToOctreeIt);
                    }

                    m_entries.Erase(it);
                }

                parentInsertResult = parent->Move(value, aabb, allowRebuild, parent->m_entries.End());

                break;
            }

            parent = parent->m_parent;
        }

        if (parentInsertResult.HasValue())
        { // succesfully inserted, safe to call CollapseParents()
            // Entry has now been added to it's appropriate octant which is a parent of this -
            // collapse this up to the top
            CollapseParents(allowRebuild);

            return parentInsertResult.Get();
        }

        // not inserted because no Move() was called on parents (because they don't contain AABB),
        // have to _manually_ call Move() which will go to the above branch for the parent octant,
        // this invalidating `this`

        Assert(lastParent != nullptr);

        return lastParent->Move(value, aabb, allowRebuild, lastParent->m_entries.End());
    }

    // CONTAINS AABB HERE

    if (allowRebuild)
    {
        // Check if we can go deeper.
        for (Octant& octant : m_octants)
        {
            if (octant.aabb.Contains(newAabb))
            {
                if (it != m_entries.End())
                {
                    if (m_state != nullptr)
                    {
                        auto entryToOctreeIt = m_state->entryToOctree.Find(value);

                        Assert(entryToOctreeIt != m_state->entryToOctree.End());
                        Assert(entryToOctreeIt->second == this);

                        m_state->entryToOctree.Erase(entryToOctreeIt);
                    }

                    m_entries.Erase(it);
                }

                if (!IsDivided())
                {
                    if (allowRebuild && m_octantId.GetDepth() < OctantId::maxDepth - 1)
                    {
                        Divide();
                    }
                    else
                    {
                        continue;
                    }
                }

                Assert(octant.octree != nullptr);

                const auto octantMoveResult = octant.octree->Move(value, aabb, allowRebuild, octant.octree->m_entries.End());
                Assert(octantMoveResult.first);

                return octantMoveResult;
            }
        }
    }
    else
    {
        m_state->MarkOctantDirty(m_octantId, true);
    }

    if (it != m_entries.End())
    { /* Not moved out of this octant (for now) */
        it->aabb = newAabb;
    }
    else
    { /* Moved into this octant */
        if ((m_flags & OctreeFlags::OF_ONLY_INSERT_INTO_LEAF_NODES) && IsDivided())
        {
            // only allowed to stay here for a little bit, make sure next Rebuild() call will update this
            m_state->MarkOctantDirty(m_octantId, true);
        }

        m_entries.Insert(Entry { value, newAabb });

        if (m_state != nullptr)
        {
            Assert(m_state->entryToOctree.Find(value) == m_state->entryToOctree.End(), "Entry must not already be in octree hierarchy.");

            m_state->entryToOctree.Insert(value, this);
        }
    }

    return {
        { OctreeBase<Derived, TEntry>::Result::OCTREE_OK },
        m_octantId
    };
}

template <class Derived, class TEntry>
OctreeBase<Derived, TEntry>::InsertResult OctreeBase<Derived, TEntry>::Update(const TEntry& value, const BoundingBox& aabb, bool forceInvalidation, bool allowRebuild)
{
    if (m_state != nullptr)
    {
        const auto it = m_state->entryToOctree.Find(value);

        if (it == m_state->entryToOctree.End())
        {
            return {
                { Result::OCTREE_ERR, "Object not found in entry map!" },
                OctantId::Invalid()
            };
        }

        if (OctreeBase* octree = it->second)
        {
            return octree->Update_Internal(value, aabb, forceInvalidation, allowRebuild);
        }

        return {
            { Result::OCTREE_ERR, "Object has no octree in entry map!" },
            OctantId::Invalid()
        };
    }

    return Update_Internal(value, aabb, forceInvalidation, allowRebuild);
}

template <class Derived, class TEntry>
OctreeBase<Derived, TEntry>::InsertResult OctreeBase<Derived, TEntry>::Update_Internal(const TEntry& value, const BoundingBox& aabb, bool forceInvalidation, bool allowRebuild)
{
    const auto it = m_entries.Find(value);

    if (it == m_entries.End())
    {
        if (m_isDivided)
        {
            for (Octant& octant : m_octants)
            {
                Assert(octant.octree != nullptr);

                auto updateInternalResult = octant.octree->Update_Internal(value, aabb, forceInvalidation, allowRebuild);

                if (updateInternalResult.first)
                {
                    return updateInternalResult;
                }
            }
        }

        return {
            { Result::OCTREE_ERR, "Could not update in any sub octants" },
            OctantId::Invalid()
        };
    }

    if (forceInvalidation)
    {
        // force invalidation of this entry so the octant's hash will be updated
        Invalidate();
    }

    const BoundingBox& newAabb = aabb;
    const BoundingBox& oldAabb = it->aabb;

    if (newAabb == oldAabb)
    {
        if (forceInvalidation)
        {
            // force invalidation of this entry so the octant's hash will be updated
            m_state->MarkOctantDirty(m_octantId);
        }

        /* AABB has not changed - no need to update */
        return {
            { Result::OCTREE_OK },
            m_octantId
        };
    }

    /* AABB has changed to we remove it from this octree and either:
     * If we don't contain it anymore - insert it from the highest level octree that still contains the aabb and then walking down from there
     * If we do still contain it - we will remove it from this octree and re-insert it to find the deepest child octant
     */

    return Move(value, newAabb, allowRebuild, it);
}

template <class Derived, class TEntry>
OctreeBase<Derived, TEntry>::InsertResult OctreeBase<Derived, TEntry>::Rebuild()
{
    if (IsRoot())
    {
        return Rebuild(BoundingBox::Empty(), /* allowGrow */ true);
    }
    else
    {
        // if we are not root, we can't grow this octant as it would invalidate the rules of an octree!
        return Rebuild(m_aabb, /* allowGrow */ false);
    }
}

template <class Derived, class TEntry>
OctreeBase<Derived, TEntry>::InsertResult OctreeBase<Derived, TEntry>::Rebuild(const BoundingBox& newAabb, bool allowGrow)
{
    Array<Entry> newEntries;
    Clear(newEntries, /* undivide */ true);

    m_aabb = newAabb;

    if (allowGrow)
    {
        Assert(IsRoot());

        for (auto& it : newEntries)
        {
            if (it.aabb.IsValid() && it.aabb.IsFinite())
            {
                m_aabb = m_aabb.Union(it.aabb);
            }
        }
    }

    InitOctants();

    m_state->entryToOctree.Clear();

    for (auto& it : newEntries)
    {
        if (it.aabb.IsValid() && it.aabb.IsFinite())
        {
            // should already be contained in the aabb of non-root octants
            AssertDebug(m_aabb.Contains(it.aabb));
        }

        auto insertResult = Insert(it.value, it.aabb, true /* allow rebuild */);

        if (!insertResult.first)
        {
            return insertResult;
        }
    }

#ifdef HYP_DEBUG_MODE
    if (m_flags & OctreeFlags::OF_ONLY_INSERT_INTO_LEAF_NODES)
    {
        for (auto& it : m_state->entryToOctree)
        {
            AssertDebug(it.second->IsDivided() == false, "OF_ONLY_INSERT_INTO_LEAF_NODES is set, but entry is in a divided octree");
        }
    }
#endif

    return {
        { OctreeBase<Derived, TEntry>::Result::OCTREE_OK },
        m_octantId
    };
}

template <class Derived, class TEntry>
OctreeBase<Derived, TEntry>::InsertResult OctreeBase<Derived, TEntry>::RebuildExtend_Internal(const BoundingBox& extendIncludeAabb)
{
    if (!extendIncludeAabb.IsValid())
    {
        return {
            { OctreeBase<Derived, TEntry>::Result::OCTREE_ERR, "AABB is in invalid state" },
            OctantId::Invalid()
        };
    }

    if (!extendIncludeAabb.IsFinite())
    {
        return {
            { OctreeBase<Derived, TEntry>::Result::OCTREE_ERR, "AABB is not finite" },
            OctantId::Invalid()
        };
    }

    // have to grow the aabb by rebuilding the octree
    BoundingBox newAabb(m_aabb.Union(extendIncludeAabb));
    // grow our new aabb by a predetermined growth factor,
    // to keep it from constantly resizing
    newAabb *= growthFactor;

    return Rebuild(newAabb, /* allowGrow */ false);
}

template <class Derived, class TEntry>
void OctreeBase<Derived, TEntry>::PerformUpdates()
{
    Assert(m_state != nullptr);

    if (!m_state->NeedsRebuild())
    {
        // No octant to rebuild, skipping
        return;
    }

    OctreeBase* octant = GetChildOctant(m_state->dirtyState.octantId);
    Assert(octant != nullptr);

    const auto rebuildResult = octant->Rebuild();

    if (rebuildResult.first)
    {
        // set rebuild state back to invalid if rebuild was successful
        m_state->dirtyState = {};
    }
}

template <class Derived, class TEntry>
void OctreeBase<Derived, TEntry>::CollectEntries(Array<const TEntry*>& outEntries) const
{
    outEntries.Reserve(outEntries.Size() + m_entries.Size());

    for (const OctreeBase<Derived, TEntry>::Entry& entry : m_entries)
    {
        outEntries.PushBack(&entry.value);
    }

    if (m_isDivided)
    {
        for (const Octant& octant : m_octants)
        {
            Assert(octant.octree != nullptr);

            octant.octree->CollectEntries(outEntries);
        }
    }
}

template <class Derived, class TEntry>
void OctreeBase<Derived, TEntry>::CollectEntries(const BoundingSphere& bounds, Array<const TEntry*>& outEntries) const
{
    if (!bounds.Overlaps(m_aabb))
    {
        return;
    }

    outEntries.Reserve(outEntries.Size() + m_entries.Size());

    for (const OctreeBase<Derived, TEntry>::Entry& entry : m_entries)
    {
        if (bounds.Overlaps(entry.aabb))
        {
            outEntries.PushBack(&entry.value);
        }
    }

    if (m_isDivided)
    {
        for (const Octant& octant : m_octants)
        {
            Assert(octant.octree != nullptr);

            octant.octree->CollectEntries(bounds, outEntries);
        }
    }
}

template <class Derived, class TEntry>
void OctreeBase<Derived, TEntry>::CollectEntries(const BoundingBox& bounds, Array<const TEntry*>& outEntries) const
{
    if (!m_aabb.Overlaps(bounds))
    {
        return;
    }

    outEntries.Reserve(outEntries.Size() + m_entries.Size());

    for (const OctreeBase<Derived, TEntry>::Entry& entry : m_entries)
    {
        if (bounds.Overlaps(entry.aabb))
        {
            outEntries.PushBack(&entry.value);
        }
    }

    if (m_isDivided)
    {
        for (const Octant& octant : m_octants)
        {
            Assert(octant.octree != nullptr);

            octant.octree->CollectEntries(bounds, outEntries);
        }
    }
}

template <class Derived, class TEntry>
bool OctreeBase<Derived, TEntry>::GetNearestOctants(const Vec3f& position, FixedArray<Derived*, 8>& out) const
{
    if (!m_aabb.ContainsPoint(position))
    {
        return false;
    }

    if (!m_isDivided)
    {
        return false;
    }

    for (const Octant& octant : m_octants)
    {
        Assert(octant.octree != nullptr);

        if (octant.octree->GetNearestOctants(position, out))
        {
            return true;
        }
    }

    for (SizeType i = 0; i < m_octants.Size(); i++)
    {
        out[i] = static_cast<Derived*>(m_octants[i].octree.Get());
    }

    return true;
}

template <class Derived, class TEntry>
bool OctreeBase<Derived, TEntry>::GetNearestOctant(const Vec3f& position, Derived const*& out) const
{
    if (!m_aabb.ContainsPoint(position))
    {
        return false;
    }

    if (m_isDivided)
    {
        for (const Octant& octant : m_octants)
        {
            Assert(octant.octree != nullptr);

            if (octant.octree->GetNearestOctant(position, out))
            {
                return true;
            }
        }
    }

    out = static_cast<const Derived*>(this);

    return true;
}

template <class Derived, class TEntry>
bool OctreeBase<Derived, TEntry>::GetFittingOctant(const BoundingBox& aabb, Derived const*& out) const
{
    if (!m_aabb.Contains(aabb))
    {
        out = nullptr;

        return false;
    }

    if (m_isDivided)
    {
        for (const Octant& octant : m_octants)
        {
            Assert(octant.octree != nullptr);

            if (octant.octree->GetFittingOctant(aabb, out))
            {
                return true;
            }
        }
    }

    out = static_cast<const Derived*>(this);

    return true;
}
