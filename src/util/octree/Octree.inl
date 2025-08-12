template <class Derived, class Payload>
const BoundingBox OctreeBase<Derived, Payload>::g_defaultBounds = BoundingBox({ -250.0f }, { 250.0f });

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
OctreeBase<Derived, Payload>::OctreeBase()
    : OctreeBase(Derived::g_defaultBounds)
{
    static_assert(Derived::g_maxDepth <= OctantId::maxDepth);
}

template <class Derived, class Payload>
OctreeBase<Derived, Payload>::OctreeBase(const BoundingBox& aabb)
    : OctreeBase(nullptr, aabb, 0)
{
    m_state = Derived::CreateOctreeState();
}

template <class Derived, class Payload>
OctreeBase<Derived, Payload>::OctreeBase(Derived* parent, const BoundingBox& aabb, uint8 index)
    : m_parent(nullptr),
      m_aabb(aabb),
      m_isDivided(false),
      m_state(nullptr),
      m_octantId(index, OctantId::Invalid()),
      m_invalidationMarker(0),
      m_payload {}
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

    for (Octant& octant : m_octants)
    {
        if (octant.octree != nullptr)
        {
            delete octant.octree;
            octant.octree = nullptr;
        }
    }
}

template <class Derived, class Payload>
void OctreeBase<Derived, Payload>::SetParent(Derived* parent)
{
    m_parent = parent;

    if (m_parent != nullptr)
    {
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

            octant.octree->SetParent(static_cast<Derived*>(this));
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
Derived* OctreeBase<Derived, Payload>::GetChildOctant(OctantId octantId)
{
    if (octantId == OctantId::Invalid())
    {
        return nullptr;
    }

    if (octantId == m_octantId)
    {
        return static_cast<Derived*>(this);
    }

    if (octantId.depth <= m_octantId.depth)
    {
        return nullptr;
    }

    Derived* current = static_cast<Derived*>(this);

    for (uint32 depth = m_octantId.depth + 1; depth <= octantId.depth; depth++)
    {
        const uint8 index = octantId.GetIndex(depth);

        if (!current || !current->IsDivided())
        {
            return nullptr;
        }

        current = current->m_octants[index].octree;
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

        octant.octree = Derived::CreateChildOctant(static_cast<Derived*>(this), octant.aabb, uint8(i));
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

        delete octant.octree;
        octant.octree = nullptr;
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

    Derived* iteration = m_parent;
    Derived* highestEmpty = nullptr;

    while (iteration != nullptr && iteration->Empty())
    {
        for (const Octant& octant : iteration->m_octants)
        {
            Assert(octant.octree != nullptr);

            if (octant.octree == highestEmpty)
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
    outPayloads.Reserve(outPayloads.Size() + 1);

    if (!m_payload.Empty())
    {
        outPayloads.PushBack(std::move(m_payload));
        m_payload = Payload {};
    }

    if (!m_isDivided)
    {
        return;
    }

    for (Octant& octant : m_octants)
    {
        Assert(octant.octree != nullptr);

        octant.octree->Clear(outPayloads, /* undivide */ false);
    }

    if (undivide && IsDivided())
    {
        Undivide();
    }
}

template <class Derived, class Payload>
OctreeBase<Derived, Payload>::Result OctreeBase<Derived, Payload>::Insert(const Payload& payload, const BoundingBox& aabb)
{
    if (Derived::g_flags & OF_INSERT_ON_OVERLAP)
    {
        AssertDebug(aabb.IsValid() && aabb.IsFinite() && !aabb.IsZero(), "Attempting to insert invalid AABB into octree: {}", aabb);
    }

    if (aabb.IsValid() && aabb.IsFinite())
    {
        if (!m_aabb.Overlaps(aabb))
        {
            return HYP_MAKE_ERROR(Error, "Entry AABB outside of octant AABB");
        }

        // stop recursing if we are at max depth
        if (m_octantId.GetDepth() < int(Derived::g_maxDepth) - 1)
        {
            bool wasInserted = false;

            for (Octant& octant : m_octants)
            {
                if (!((Derived::g_flags & OF_INSERT_ON_OVERLAP) ? octant.aabb.Overlaps(aabb) : octant.aabb.Contains(aabb)))
                {
                    continue;
                }

                if (!IsDivided())
                {
                    Divide();
                }

                Assert(octant.octree != nullptr);

                Result insertResult = octant.octree->Insert(payload, aabb);
                wasInserted |= bool(insertResult.HasValue());

                if (Derived::g_flags & OF_INSERT_ON_OVERLAP)
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

    m_state->MarkOctantDirty(m_octantId);

    m_payload = payload;

    return m_octantId;
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
        out[i] = m_octants[i].octree;
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

    // out should always be set to something, boolean indicates whether or not it was actually contained
    out = static_cast<const Derived*>(this);

    return ContainsAabb(aabb);
}
