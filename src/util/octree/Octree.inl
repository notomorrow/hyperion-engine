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
    m_state = Derived::CreateOctreeState();
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
      m_maxDepth(uint8(OctantId::maxDepth)),
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
    if (m_flags[OctreeFlags::OF_INSERT_ON_OVERLAP])
    {
        AssertDebug(aabb.IsValid() && aabb.IsFinite() && !aabb.IsZero(), "Attempting to insert invalid AABB into Octree: {}", aabb);
    }

    if (aabb.IsValid() && aabb.IsFinite())
    {
        if (!m_aabb.Overlaps(aabb))
        {
            return HYP_MAKE_ERROR(Error, "Entry AABB outside of octant AABB");
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
                    Divide();
                }

                Assert(octant.octree != nullptr);

                Result insertResult = octant.octree->Insert(payload, aabb);
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
