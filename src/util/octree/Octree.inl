template <class Derived, class Payload>
const BoundingBox OctreeBase<Derived, Payload>::defaultBounds = BoundingBox({ -250.0f }, { 250.0f });

template <class Derived, class Payload>
void OctreeState<Derived, Payload>::MarkOctantDirty(OctantId octantId, bool needsRebuild)
{
    const DirtyState prevDirtyState = dirtyState;

    if (octantId.IsInvalid())
    {
        return;
    }

    if (dirtyState.octantId.IsInvalid())
    {
        dirtyState = { octantId, needsRebuild };

        return;
    }

    while (octantId != dirtyState.octantId && !octantId.IsChildOf(dirtyState.octantId) && !dirtyState.octantId.IsInvalid())
    {
        dirtyState.octantId = dirtyState.octantId.GetParent();
    }

    // should always end up at root if it doesnt match any
    AssertDebug(dirtyState.octantId != OctantId::Invalid());
    dirtyState.needsRebuild |= needsRebuild;
}

template <class Derived, class Payload>
OctreeBase<Derived, Payload>::OctreeBase(EnumFlags<OctreeFlags> flags, uint8 maxDepth)
    : OctreeBase(defaultBounds)
{
    m_flags = flags;
    m_maxDepth = maxDepth;

    if (m_maxDepth > uint8(OctantId::maxDepth))
    {
        m_maxDepth = uint8(OctantId::maxDepth);
    }
}

template <class Derived, class Payload>
OctreeBase<Derived, Payload>::OctreeBase(const BoundingBox& aabb)
    : OctreeBase(aabb, nullptr, 0)
{
    m_state = new OctreeState<Derived, Payload>();

    if (UseEntryToOctantMap())
    {
        m_state->entryToOctant = new HashMap<TEntry, OctreeBase<Derived, Payload>*>();
    }
}

template <class Derived, class Payload>
OctreeBase<Derived, Payload>::OctreeBase(const BoundingBox& aabb, OctreeBase* parent, uint8 index)
    : m_aabb(aabb),
      m_parent(nullptr),
      m_isDivided(false),
      m_state(nullptr),
      m_octantId(index, OctantId::Invalid()),
      m_invalidationMarker(0),
      m_flags(OctreeFlags::OF_DEFAULT),
      m_maxDepth(uint8(OctantId::maxDepth))
{
    if (parent != nullptr)
    {
        SetParent(parent); // call explicitly to set root ptr
    }

    Assert(m_octantId.GetIndex() == index);

    InitOctants();
}

template <class Derived, class Payload>
OctreeBase<Derived, Payload>::~OctreeBase()
{
    if (IsRoot())
    {
        delete m_state;
    }
}

template <class Derived, class Payload>
void OctreeBase<Derived, Payload>::SetParent(OctreeBase* parent)
{
    m_parent = parent;

    if (m_parent != nullptr)
    {
        m_flags = parent->m_flags;
        m_maxDepth = parent->m_maxDepth;
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

template <class Derived, class Payload>
bool OctreeBase<Derived, Payload>::EmptyDeep(int depth, uint8 octantMask) const
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

template <class Derived, class Payload>
void OctreeBase<Derived, Payload>::InitOctants()
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

template <class Derived, class Payload>
OctreeBase<Derived, Payload>* OctreeBase<Derived, Payload>::GetChildOctant(OctantId octantId)
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

template <class Derived, class Payload>
void OctreeBase<Derived, Payload>::Divide()
{
    Assert(!IsDivided());

    for (int i = 0; i < 8; i++)
    {
        Octant& octant = m_octants[i];
        Assert(octant.octree == nullptr);

        octant.octree = CreateChildOctant(octant.aabb, static_cast<Derived*>(this), uint8(i));
    }

    m_isDivided = true;
}

template <class Derived, class Payload>
void OctreeBase<Derived, Payload>::Undivide()
{
    Assert(IsDivided());
    Assert(m_payload.Empty(), "Undivide() should be called on octrees with no payload");

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

template <class Derived, class Payload>
void OctreeBase<Derived, Payload>::Invalidate()
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

template <class Derived, class Payload>
void OctreeBase<Derived, Payload>::CollapseParents(bool allowRebuild)
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

template <class Derived, class Payload>
void OctreeBase<Derived, Payload>::Clear()
{
    Array<Payload> payloads;
    Clear(payloads, /* undivide */ true);
}

template <class Derived, class Payload>
void OctreeBase<Derived, Payload>::Clear(Array<Payload>& outPayloads, bool undivide)
{
    outPaylods.Reserve(outPaylods.Size() + 1);

    if (!m_isDivided)
    {
        return;
    }

    for (Octant& octant : m_octants)
    {
        Assert(octant.octree != nullptr);

        octant.octree->Clear(outPaylods, /* undivide */ false);
    }

    if (undivide && IsDivided())
    {
        Undivide();
    }
}

template <class Derived, class Payload>
OctreeBase<Derived, Payload>::Result OctreeBase<Derived, Payload>::Insert(const TEntry& value, const BoundingBox& aabb, bool allowRebuild)
{
    if (m_flags[OctreeFlags::OF_INSERT_ON_OVERLAP])
    {
        AssertDebug(aabb.IsValid() && aabb.IsFinite() && !aabb.IsZero(), "Attempting to insert invalid AABB into Octree: {}", aabb);
    }

    if (aabb.IsValid() && aabb.IsFinite())
    {
        if (IsRoot())
        {
            if (!m_aabb.Contains(aabb) && (m_flags[OctreeFlags::OF_ALLOW_GROW_ROOT]))
            {
                if (allowRebuild)
                {
                    auto rebuildResult = RebuildExtend_Internal(aabb);

                    if (rebuildResult.HasError())
                    {
                        return rebuildResult;
                    }
                }
                else
                {
                    // mark octree to be rebuilt
                    m_state->MarkOctantDirty(m_octantId, true);
                }
            }
        }
        else
        {
            if (!m_aabb.Overlaps(aabb))
            {
                return HYP_MAKE_ERROR(Error, "Entry AABB outside of octant AABB");
            }
        }

        // stop recursing if we are at max depth
        if (m_octantId.GetDepth() < m_maxDepth - 1)
        {
            bool wasInserted = false;

            for (Octant& octant : m_octants)
            {
                if (!(m_flags[OctreeFlags::OF_INSERT_ON_OVERLAP] ? octant.aabb.Overlaps(aabb) : octant.aabb.Contains(aabb)))
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

                        // insert into parent for now (will be rebuilt)
                        return Insert_Internal(value, aabb);
                    }

                    Divide();
                }

                Assert(octant.octree != nullptr);

                Result insertResult = octant.octree->Insert(value, aabb, allowRebuild);
                wasInserted |= bool(insertResult.HasValue());

                if (m_flags[OctreeFlags::OF_INSERT_ON_OVERLAP])
                {
                    AssertDebug(insertResult.HasValue(), "Failed to insert into overlapping octant! Message: {}", insertResult.GetError().GetMessage());
                }
                else
                {
                    // return on first call to Insert() on child octant - child fully contains the aabb
                    return insertResult;
                }
            }

            if (wasInserted)
            {
                return m_octantId;
            }
        }
    }

    return Insert_Internal(value, aabb);
}

template <class Derived, class Payload>
OctreeBase<Derived, Payload>::Result OctreeBase<Derived, Payload>::Insert_Internal(const TEntry& value, const BoundingBox& aabb)
{
    if (UseEntryToOctantMap())
    {
        if (m_state->entryToOctant->Find(value) != m_state->entryToOctant->End())
        {
            return HYP_MAKE_ERROR(Error, "Entry already exists in entry map");
        }

        (*m_state->entryToOctant)[value] = this;
    }

    m_entries.Set(Entry { value, aabb });

    // mark dirty (not for rebuild)
    m_state->MarkOctantDirty(m_octantId);

    return m_octantId;
}

template <class Derived, class Payload>
OctreeBase<Derived, Payload>::Result OctreeBase<Derived, Payload>::Remove(const TEntry& value, bool allowRebuild)
{
    if (UseEntryToOctantMap())
    {
        const auto it = m_state->entryToOctant->Find(value);

        if (it != m_state->entryToOctant->End())
        {
            if (OctreeBase* octant = it->second)
            {
                return octant->Remove_Internal(value, allowRebuild);
            }
        }
    }

    return Remove_Internal(value, allowRebuild);
}

template <class Derived, class Payload>
OctreeBase<Derived, Payload>::Result OctreeBase<Derived, Payload>::Remove_Internal(const TEntry& value, bool allowRebuild)
{
    const auto it = m_entries.Find(value);

    if (it == m_entries.End())
    {
        if (m_isDivided)
        {
            bool wasRemoved = false;

            for (Octant& octant : m_octants)
            {
                Assert(octant.octree != nullptr);

                if (m_flags & OctreeFlags::OF_INSERT_ON_OVERLAP)
                {
                    wasRemoved |= bool(octant.octree->Remove_Internal(value, allowRebuild));
                }
                else
                {
                    if (Result octantResult = octant.octree->Remove_Internal(value, allowRebuild))
                    {
                        return octantResult;
                    }
                }
            }

            if (wasRemoved)
            {
                return m_octantId;
            }
        }

        return HYP_MAKE_ERROR(Error, "Could not be removed from any sub octants and not found in this octant");
    }

    if (UseEntryToOctantMap())
    {
        auto entryToOctantIt = m_state->entryToOctant->Find(value);

        if (entryToOctantIt != m_state->entryToOctant->End())
        {
            m_state->entryToOctant->Erase(entryToOctantIt);
        }
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

    return m_octantId;
}

template <class Derived, class Payload>
OctreeBase<Derived, Payload>::Result OctreeBase<Derived, Payload>::Move(const TEntry& value, const BoundingBox& aabb, bool allowRebuild, EntrySet::Iterator it)
{
    const BoundingBox& newAabb = aabb;

    const bool isRoot = IsRoot();
    const bool contains = ContainsAabb(aabb);

    if (!contains)
    {
        // NO LONGER CONTAINS AABB

        if (isRoot)
        {
            // have to rebuild, invalidating child octants.
            // which we have a ContainsAabb() check for child entries walking upwards

            if (allowRebuild)
            {
                return RebuildExtend_Internal(newAabb);
            }
            else
            {
                m_state->MarkOctantDirty(m_octantId, true);

                // Moved outside of the root octree, but we keep it here for now.
                // Next call of PerformUpdates(), we will extend the octree.
                return m_octantId;
            }
        }

        // not root

        Optional<Result> parentInsertResult;

        /* Contains is false at this point */
        OctreeBase* parent = m_parent;
        OctreeBase* lastParent = m_parent;

        while (parent != nullptr)
        {
            lastParent = parent;

            if (parent->ContainsAabb(newAabb))
            {
                if (it != m_entries.End())
                {
                    if (UseEntryToOctantMap())
                    {
                        auto entryToOctantIt = m_state->entryToOctant->Find(value);

                        if (entryToOctantIt != m_state->entryToOctant->End())
                        {
                            m_state->entryToOctant->Erase(entryToOctantIt);
                        }
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
        bool wasMoved = false;

        // Check if we can go deeper.
        for (Octant& octant : m_octants)
        {
            if (m_flags[OctreeFlags::OF_INSERT_ON_OVERLAP] ? octant.aabb.Overlaps(newAabb) : octant.aabb.Contains(newAabb))
            {
                if (it != m_entries.End())
                {
                    if (UseEntryToOctantMap())
                    {
                        auto entryToOctantIt = m_state->entryToOctant->Find(value);

                        if (entryToOctantIt != m_state->entryToOctant->End())
                        {
                            m_state->entryToOctant->Erase(entryToOctantIt);
                        }
                    }

                    m_entries.Erase(it);
                }

                if (!IsDivided())
                {
                    if (allowRebuild && m_octantId.GetDepth() < m_maxDepth - 1)
                    {
                        Divide();
                    }
                    else
                    {
                        // no point checking other octants
                        break;
                    }
                }

                AssertDebug(octant.octree != nullptr);

                if (m_flags & OctreeFlags::OF_INSERT_ON_OVERLAP)
                {
                    wasMoved |= bool(octant.octree->Move(value, aabb, allowRebuild, octant.octree->m_entries.End()));
                }
                else
                {
                    return octant.octree->Move(value, aabb, allowRebuild, octant.octree->m_entries.End());
                }
            }
        }

        if (wasMoved)
        {
            return m_octantId;
        }
    }
    else
    {
        m_state->MarkOctantDirty(m_octantId, true);
    }

    if (it != m_entries.End())
    {
        /* Not moved out of this octant (for now) */
        it->aabb = newAabb;
    }
    else
    {
        /* Moved into this octant */
        m_entries.Insert(Entry { value, newAabb });

        if (UseEntryToOctantMap())
        {
            (*m_state->entryToOctant)[value] = this;
        }
    }

    return m_octantId;
}

template <class Derived, class Payload>
OctreeBase<Derived, Payload>::Result OctreeBase<Derived, Payload>::Update(const TEntry& value, const BoundingBox& aabb, bool forceInvalidation, bool allowRebuild)
{
    if (UseEntryToOctantMap())
    {
        const auto it = m_state->entryToOctant->Find(value);

        if (it == m_state->entryToOctant->End())
        {
            return HYP_MAKE_ERROR(Error, "Object not found in entry map!");
        }

        if (OctreeBase* octree = it->second)
        {
            return octree->Update_Internal(value, aabb, forceInvalidation, allowRebuild);
        }

        return HYP_MAKE_ERROR(Error, "Object has no octree in entry map!");
    }

    return Update_Internal(value, aabb, forceInvalidation, allowRebuild);
}

template <class Derived, class Payload>
OctreeBase<Derived, Payload>::Result OctreeBase<Derived, Payload>::Update_Internal(const TEntry& value, const BoundingBox& aabb, bool forceInvalidation, bool allowRebuild)
{
    const auto it = m_entries.Find(value);

    if (it == m_entries.End())
    {
        if (m_isDivided)
        {
            bool wasUpdated = false;

            for (Octant& octant : m_octants)
            {
                Assert(octant.octree != nullptr);

                if (m_flags & OctreeFlags::OF_INSERT_ON_OVERLAP)
                {
                    wasUpdated |= bool(octant.octree->Update_Internal(value, aabb, forceInvalidation, allowRebuild));
                }
                else
                {
                    Result updateInternalResult = octant.octree->Update_Internal(value, aabb, forceInvalidation, allowRebuild);

                    if (updateInternalResult.HasValue())
                    {
                        return updateInternalResult;
                    }
                }
            }

            if (wasUpdated)
            {
                return m_octantId;
            }
        }

        return HYP_MAKE_ERROR(Error, "Could not update in any sub octants");
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
        return m_octantId;
    }

    /* AABB has changed to we remove it from this octree and either:
     * If we don't contain it anymore - insert it from the highest level octree that still contains the aabb and then walking down from there
     * If we do still contain it - we will remove it from this octree and re-insert it to find the deepest child octant
     */

    return Move(value, newAabb, allowRebuild, it);
}

template <class Derived, class Payload>
OctreeBase<Derived, Payload>::Result OctreeBase<Derived, Payload>::Rebuild()
{
    if (IsRoot())
    {
        return static_cast<Derived*>(this)->Rebuild(BoundingBox::Empty(), /* allowGrow */ m_flags[OctreeFlags::OF_ALLOW_GROW_ROOT]);
    }
    else
    {
        // if we are not root, we can't grow this octant as it would invalidate the rules of an octree!
        return static_cast<Derived*>(this)->Rebuild(m_aabb, /* allowGrow */ false);
    }
}

template <class Derived, class Payload>
OctreeBase<Derived, Payload>::Result OctreeBase<Derived, Payload>::Rebuild(const BoundingBox& newAabb, bool allowGrow)
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

    if (IsRoot() && UseEntryToOctantMap())
    {
        m_state->entryToOctant->Clear();
    }

    for (auto& it : newEntries)
    {
        if (it.aabb.IsValid() && it.aabb.IsFinite())
        {
            // should already be contained in the aabb of non-root octants
            AssertDebug(ContainsAabb(it.aabb));
        }

        Result insertResult = Insert(it.value, it.aabb, true /* allow rebuild */);

        if (insertResult.HasError())
        {
            return insertResult;
        }
    }

    return m_octantId;
}

template <class Derived, class Payload>
OctreeBase<Derived, Payload>::Result OctreeBase<Derived, Payload>::RebuildExtend_Internal(const BoundingBox& extendIncludeAabb)
{
    if (!extendIncludeAabb.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "AABB is in invalid state");
    }

    if (!extendIncludeAabb.IsFinite())
    {
        return HYP_MAKE_ERROR(Error, "AABB is not finite");
    }

    // have to grow the aabb by rebuilding the octree
    BoundingBox newAabb(m_aabb.Union(extendIncludeAabb));
    // grow our new aabb by a predetermined growth factor,
    // to keep it from constantly resizing
    newAabb *= growthFactor;

    return Rebuild(newAabb, /* allowGrow */ false);
}

template <class Derived, class Payload>
void OctreeBase<Derived, Payload>::PerformUpdates()
{
    Assert(m_state != nullptr);

    if (!m_state->NeedsRebuild())
    {
        // No octant to rebuild, skipping
        return;
    }

    OctreeBase* octant = GetChildOctant(m_state->dirtyState.octantId);
    Assert(octant != nullptr);

    const Result rebuildResult = octant->Rebuild();

    if (!rebuildResult.HasError())
    {
        // set rebuild state back to invalid if rebuild was successful
        m_state->dirtyState = {};
    }
}

template <class Derived, class Payload>
bool OctreeBase<Derived, Payload>::GetNearestOctants(const Vec3f& position, FixedArray<Derived*, 8>& out) const
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

template <class Derived, class Payload>
bool OctreeBase<Derived, Payload>::GetNearestOctant(const Vec3f& position, Derived const*& out) const
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

template <class Derived, class Payload>
bool OctreeBase<Derived, Payload>::GetFittingOctant(const BoundingBox& aabb, Derived const*& out) const
{
    if (!ContainsAabb(aabb))
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
