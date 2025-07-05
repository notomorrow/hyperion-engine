/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RESOURCE_TRACKER_HPP
#define HYPERION_RESOURCE_TRACKER_HPP

#include <core/object/ObjId.hpp>
#include <core/Defines.hpp>

#include <core/containers/SparsePagedArray.hpp>
#include <core/containers/HashMap.hpp>

#include <core/utilities/TypeId.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Types.hpp>

namespace hyperion {

HYP_API extern SizeType GetNumDescendants(TypeId typeId);
HYP_API extern int GetSubclassIndex(TypeId baseTypeId, TypeId subclassTypeId);

/*! \brief The action to take on call to Advance for a ResourceTracker. */
enum class AdvanceAction : uint32
{
    CLEAR,  //! Clear the 'next' elements, so on next iteration, any elements that have not been re-added are marked for removal.
    PERSIST //! Copy the previous elements over to next. To remove elements, `RemoveProxy` needs to be manually called.
};

template <class IdType, class ElementType>
class ResourceTracker
{
    template <class Derived, bool IsConst>
    struct IteratorBase
    {
        using ResourceTrackerType = std::conditional_t<IsConst, const ResourceTracker, ResourceTracker>;

        ResourceTrackerType* tracker;

        // index into m_subclassImpls (and m_subclassImplsInitialized)
        SizeType subclassImplIndex;
        SizeType elementIndex;

        IteratorBase(ResourceTrackerType* tracker, SizeType subclassIdx, SizeType elemIdx)
            : tracker(tracker),
              subclassImplIndex(subclassIdx),
              elementIndex(elemIdx)
        {
            if (subclassImplIndex == Bitset::notFound && elementIndex == Bitset::notFound)
            {
                // both set to notFound : at end
                return;
            }

            // find valid element - first check m_impl then try subclasses if not found
            if (subclassImplIndex == Bitset::notFound)
            {
                elementIndex = FindNextSet(tracker->m_impl.previous, elementIndex);

                if (elementIndex != Bitset::notFound)
                {
                    // We found an element at the given index in our main impl.
                    return;
                }

                // Need to search subclasses - set indices to zero so we land on the first subclass that is valid and has a valid element
                subclassImplIndex = 0;
                elementIndex = 0;
            }

            // Find first subclass that is set (if it exists),
            // next part will handle finding the element index.
            subclassImplIndex = FindNextSet(tracker->m_subclassImplsInitialized, subclassImplIndex);

            while (subclassImplIndex != Bitset::notFound)
            {
                AssertDebug(subclassImplIndex < tracker->m_subclassImpls.Size());

                auto& impl = tracker->m_subclassImpls[subclassImplIndex].Get();
                elementIndex = FindNextSet(impl.previous, elementIndex);

                // Found valid element in subclass impl.
                if (elementIndex != Bitset::notFound)
                {
                    return;
                }

                // If we are at the end of the current subclass impl, move to the next one.
                subclassImplIndex = FindNextSet(tracker->m_subclassImplsInitialized, subclassImplIndex + 1);
                elementIndex = 0;
            }

            // exhausted all subclass elements
            subclassImplIndex = Bitset::notFound;
            elementIndex = Bitset::notFound;
        }

        Derived& operator++()
        {
            // at the end check
            if (subclassImplIndex == Bitset::notFound && elementIndex == Bitset::notFound)
            {
                HYP_FAIL("At end! cannot increment");
            }

            if (subclassImplIndex == Bitset::notFound)
            {
                elementIndex = FindNextSet(tracker->m_impl.previous, elementIndex + 1);

                if (elementIndex != Bitset::notFound)
                {
                    // We found an element at the given index in our main impl.
                    return static_cast<Derived&>(*this);
                }

                // Find first subclass that is set (if it exists),
                // next part will handle finding the element index.
                subclassImplIndex = FindNextSet(tracker->m_subclassImplsInitialized, 0);
                elementIndex = SizeType(-1); // wrap around for next
            }

            while (subclassImplIndex != Bitset::notFound)
            {
                AssertDebug(subclassImplIndex < tracker->m_subclassImpls.Size());

                auto& impl = tracker->m_subclassImpls[subclassImplIndex].Get();

                elementIndex = FindNextSet(impl.previous, elementIndex + 1);

                if (elementIndex != Bitset::notFound)
                {
                    // Found a valid subclass element with valid element
                    return static_cast<Derived&>(*this);
                }

                // If we are at the end of the current subclass impl, move to the next one.
                subclassImplIndex = FindNextSet(tracker->m_subclassImplsInitialized, subclassImplIndex + 1);
                elementIndex = SizeType(-1); // wrap around for next
            }

            // exhausted all subclass elements
            subclassImplIndex = Bitset::notFound;
            elementIndex = Bitset::notFound;

            return static_cast<Derived&>(*this);
        }

        Derived operator++(int)
        {
            Derived tmp = static_cast<Derived>(*this);
            ++(*this);
            return tmp;
        }

        auto operator*() const -> std::conditional_t<IsConst, const ElementType, ElementType>&
        {
            if (subclassImplIndex == Bitset::notFound && elementIndex == Bitset::notFound)
            {
                HYP_FAIL("At end! cannot dereference!");
            }

            if (subclassImplIndex == Bitset::notFound) // primary search phase
            {
                return tracker->m_impl.elements.Get(elementIndex);
            }
            else
            {
                AssertDebug(subclassImplIndex < tracker->m_subclassImpls.Size());

                return tracker->m_subclassImpls[subclassImplIndex].Get().elements.Get(elementIndex);
            }

            HYP_FAIL("Invalid iterator phase");
        }

        auto operator->() const -> std::conditional_t<IsConst, const ElementType, ElementType>*
        {
            return &(**this);
        }

        bool operator==(const Derived& other) const
        {
            return tracker == other.tracker
                && subclassImplIndex == other.subclassImplIndex
                && elementIndex == other.elementIndex;
        }

        bool operator!=(const Derived& other) const
        {
            return tracker != other.tracker
                || subclassImplIndex != other.subclassImplIndex
                || elementIndex != other.elementIndex;
        }

    private:
        HYP_FORCE_INLINE bool IsAtEnd() const
        {
            return subclassImplIndex == Bitset::notFound
                && elementIndex == Bitset::notFound;
        }

        HYP_FORCE_INLINE static Bitset::BitIndex FindNextSet(const Bitset& bs, Bitset::BitIndex idx)
        {
            return bs.NextSetBitIndex(idx);
        }
    };

public:
    struct ConstIterator;

    struct Iterator : IteratorBase<Iterator, false>
    {
        Iterator() = delete;

        Iterator(ResourceTracker* tracker, SizeType subclassImplIndex, SizeType elementIndex)
            : IteratorBase<Iterator, false>(tracker, subclassImplIndex, elementIndex)
        {
        }

        // Allow implicit conversion to ConstIterator
        operator ConstIterator() const
        {
            return ConstIterator(
                IteratorBase<Iterator, false>::tracker,
                IteratorBase<Iterator, false>::subclassImplIndex,
                IteratorBase<Iterator, false>::elementIndex);
        }
    };

    struct ConstIterator : IteratorBase<ConstIterator, true>
    {
        ConstIterator() = delete;

        ConstIterator(const ResourceTracker* tracker, SizeType subclassImplIndex, SizeType elementIndex)
            : IteratorBase<ConstIterator, true>(tracker, subclassImplIndex, elementIndex)
        {
        }
    };

    enum ResourceTrackState : uint8
    {
        UNCHANGED = 0x0,
        CHANGED_ADDED = 0x1,
        CHANGED_MODIFIED = 0x2,

        CHANGED = CHANGED_ADDED | CHANGED_MODIFIED
    };

    struct Diff
    {
        uint32 numAdded = 0;
        uint32 numRemoved = 0;
        uint32 numChanged = 0;

        HYP_FORCE_INLINE bool NeedsUpdate() const
        {
            return numAdded > 0 || numRemoved > 0 || numChanged > 0;
        }
    };

    // use a sparse array so we can use IDs as indices
    // without worring about hashing for lookups and allowing us to
    // still iterate over the elements (mostly) linearly.
    using ElementArrayType = SparsePagedArray<ElementType, 256>;
    using VersionArrayType = SparsePagedArray<int, 256>; // mirrors elements array

    static_assert(std::is_base_of_v<ObjIdBase, IdType>, "IdType must be derived from ObjIdBase (must use numeric id)");

    ResourceTracker()
        : m_impl(IdType::typeIdStatic) // default impl for base class
    {
        // Setup the subclass implementations array, we initialize them as they get used
        const SizeType numDescendants = GetNumDescendants(IdType::typeIdStatic);
        m_subclassImpls.Resize(numDescendants);
    }

    ResourceTracker(const ResourceTracker& other) = delete;
    ResourceTracker& operator=(const ResourceTracker& other) = delete;

    ResourceTracker(ResourceTracker&& other) noexcept = delete;
    ResourceTracker& operator=(ResourceTracker&& other) noexcept = delete;

    ~ResourceTracker()
    {
        // destruct subtype containers
        for (Bitset::BitIndex i : m_subclassImplsInitialized)
        {
            AssertDebug(i < m_subclassImpls.Size());

            m_subclassImpls[i].Destruct();
        }
    }

    uint32 NumCurrent() const
    {
        HYP_SCOPE;

        uint32 count = m_impl.next.Count();

        for (Bitset::BitIndex i : m_subclassImplsInitialized)
        {
            AssertDebug(i < m_subclassImpls.Size());

            count += m_subclassImpls[i].Get().next.Count();
        }

        return count;
    }

    uint32 NumCurrent(TypeId typeId) const
    {
        if (typeId == IdType::typeIdStatic)
        {
            return m_impl.next.Count();
        }

        const int subclassIndex = GetSubclassIndex(m_impl.typeId, typeId);
        AssertDebug(subclassIndex >= 0, "Invalid subclass index");
        AssertDebug(subclassIndex < m_subclassImpls.Size(), "Invalid subclass index");

        if (m_subclassImplsInitialized.Test(subclassIndex))
        {
            return m_subclassImpls[subclassIndex].Get().next.Count();
        }

        return 0;
    }

    const ElementArrayType& GetElements(TypeId typeId) const
    {
        static const ElementArrayType emptyArray {};

        if (typeId == IdType::typeIdStatic)
        {
            return m_impl.elements;
        }

        const int subclassIndex = GetSubclassIndex(m_impl.typeId, typeId);
        AssertDebug(subclassIndex >= 0, "Invalid subclass index");
        AssertDebug(subclassIndex < m_subclassImpls.Size(), "Invalid subclass index");

        if (m_subclassImplsInitialized.Test(subclassIndex))
        {
            return m_subclassImpls[subclassIndex].Get().elements;
        }

        return emptyArray;
    }

    template <class T>
    const ElementArrayType& GetElements() const
    {
        static constexpr TypeId typeId = TypeId::ForType<T>();

        return GetElements(typeId);
    }

    HYP_FORCE_INLINE const Bitset& GetSubclassBits() const
    {
        return m_subclassImplsInitialized;
    }

    Diff GetDiff() const
    {
        HYP_SCOPE;

        Diff diff {};

        diff.numAdded = m_impl.GetAdded().Count();
        diff.numRemoved = m_impl.GetRemoved().Count();
        diff.numChanged = m_impl.GetChanged().Count();

        for (Bitset::BitIndex i : m_subclassImplsInitialized)
        {
            AssertDebug(i < m_subclassImpls.Size());

            diff.numAdded += m_subclassImpls[i].Get().GetAdded().Count();
            diff.numRemoved += m_subclassImpls[i].Get().GetRemoved().Count();
            diff.numChanged += m_subclassImpls[i].Get().GetChanged().Count();
        }

        return diff;
    }

    /*! \brief Marks an element to be considered valid for this frame
     *  \param id The ID of the element to track, used as a key / index
     *  \param element The element to track
     *  \param versionPtr Optional pointer to an integer that holds the version of the element. If the version does not match what is stored,
     *  the element will be considered changed and the data marked for update.
     *  \param allowDuplicatesInSameFrame If true, allows the same ID to be tracked multiple times in the same frame. (Can have different values or versions)
     */
    void Track(IdType id, const ElementType& element, const int* versionPtr = nullptr, bool allowDuplicatesInSameFrame = true)
    {
        HYP_SCOPE;

        TypeId typeId = id.GetTypeId();
        AssertDebug(typeId != TypeId::Void());

        if (typeId == m_impl.typeId)
        {
            m_impl.Track(id, element, versionPtr, allowDuplicatesInSameFrame);
            return;
        }

        const int subclassIndex = GetSubclassIndex(m_impl.typeId, typeId);
        AssertDebug(subclassIndex >= 0, "Invalid subclass index");
        AssertDebug(subclassIndex < m_subclassImpls.Size(), "Invalid subclass index");

        if (!m_subclassImplsInitialized.Test(subclassIndex))
        {
            m_subclassImpls[subclassIndex].Construct(typeId);
            m_subclassImplsInitialized.Set(subclassIndex, true);
        }

        m_subclassImpls[subclassIndex].Get().Track(id, element, versionPtr, allowDuplicatesInSameFrame);
    }

    bool MarkToKeep(IdType id)
    {
        HYP_SCOPE;

        TypeId typeId = id.GetTypeId();
        AssertDebug(typeId != TypeId::Void());

        if (typeId == m_impl.typeId)
        {
            return m_impl.MarkToKeep(id);
        }

        const int subclassIndex = GetSubclassIndex(m_impl.typeId, typeId);
        AssertDebug(subclassIndex >= 0, "Invalid subclass index");
        AssertDebug(subclassIndex < m_subclassImpls.Size(), "Invalid subclass index");

        if (!m_subclassImplsInitialized.Test(subclassIndex))
        {
            return false;
        }

        return m_subclassImpls[subclassIndex].Get().MarkToKeep(id);
    }

    void MarkToRemove(IdType id)
    {
        TypeId typeId = id.GetTypeId();
        AssertDebug(typeId != TypeId::Void());

        if (typeId == m_impl.typeId)
        {
            m_impl.MarkToRemove(id);

            return;
        }

        const int subclassIndex = GetSubclassIndex(m_impl.typeId, typeId);
        AssertDebug(subclassIndex >= 0, "Invalid subclass index");
        AssertDebug(subclassIndex < m_subclassImpls.Size(), "Invalid subclass index");

        if (!m_subclassImplsInitialized.Test(subclassIndex))
        {
            return;
        }

        m_subclassImpls[subclassIndex].Get().MarkToRemove(id);
    }

    template <class AllocatorType>
    void GetRemoved(Array<IdType, AllocatorType>& outIds, bool includeChanged) const
    {
        HYP_SCOPE;

        m_impl.GetRemoved(outIds, includeChanged);

        for (Bitset::BitIndex bitIndex : m_subclassImplsInitialized)
        {
            m_subclassImpls[bitIndex].Get().GetRemoved(outIds, includeChanged);
        }
    }

    template <class AllocatorType>
    void GetRemoved(Array<ElementType, AllocatorType>& out, bool includeChanged) const
    {
        HYP_SCOPE;

        m_impl.GetRemoved(out, includeChanged);

        for (Bitset::BitIndex i : m_subclassImplsInitialized)
        {
            m_subclassImpls[i].Get().GetRemoved(out, includeChanged);
        }
    }

    template <class AllocatorType>
    void GetRemoved(Array<ElementType*, AllocatorType>& out, bool includeChanged) const
    {
        HYP_SCOPE;

        m_impl.GetRemoved(out, includeChanged);

        for (Bitset::BitIndex i : m_subclassImplsInitialized)
        {
            m_subclassImpls[i].Get().GetRemoved(out, includeChanged);
        }
    }

    template <class AllocatorType>
    void GetAdded(Array<ElementType, AllocatorType>& out, bool includeChanged) const
    {
        HYP_SCOPE;

        m_impl.GetAdded(out, includeChanged);

        for (Bitset::BitIndex i : m_subclassImplsInitialized)
        {
            m_subclassImpls[i].Get().GetAdded(out, includeChanged);
        }
    }

    template <class AllocatorType>
    void GetAdded(Array<ElementType*, AllocatorType>& out, bool includeChanged) const
    {
        HYP_SCOPE;

        m_impl.GetAdded(out, includeChanged);

        for (Bitset::BitIndex i : m_subclassImplsInitialized)
        {
            m_subclassImpls[i].Get().GetAdded(out, includeChanged);
        }
    }

    template <class AllocatorType>
    void GetChanged(Array<IdType, AllocatorType>& outIds) const
    {
        HYP_SCOPE;

        m_impl.GetChanged(outIds);

        for (Bitset::BitIndex i : m_subclassImplsInitialized)
        {
            m_subclassImpls[i].Get().GetChanged(outIds);
        }
    }

    template <class AllocatorType>
    void GetChanged(Array<ElementType, AllocatorType>& out) const
    {
        HYP_SCOPE;

        m_impl.GetChanged(out);

        for (Bitset::BitIndex i : m_subclassImplsInitialized)
        {
            m_subclassImpls[i].Get().GetChanged(out);
        }
    }

    template <class AllocatorType>
    void GetChanged(Array<ElementType*, AllocatorType>& out) const
    {
        HYP_SCOPE;

        m_impl.GetChanged(out);

        for (Bitset::BitIndex i : m_subclassImplsInitialized)
        {
            m_subclassImpls[i].Get().GetChanged(out);
        }
    }

    template <class AllocatorType>
    void GetCurrent(Array<ElementType, AllocatorType>& out) const
    {
        HYP_SCOPE;

        m_impl.GetCurrent(out);

        for (Bitset::BitIndex i : m_subclassImplsInitialized)
        {
            m_subclassImpls[i].Get().GetCurrent(out);
        }
    }

    template <class AllocatorType>
    void GetCurrent(Array<ElementType*, AllocatorType>& out) const
    {
        HYP_SCOPE;

        m_impl.GetCurrent(out);

        for (Bitset::BitIndex i : m_subclassImplsInitialized)
        {
            m_subclassImpls[i].Get().GetCurrent(out);
        }
    }

    ElementType* GetElement(IdType id)
    {
        HYP_SCOPE;

        TypeId typeId = id.GetTypeId();

        if (typeId == m_impl.typeId)
        {
            return m_impl.GetElement(id);
        }

        const int subclassIndex = GetSubclassIndex(m_impl.typeId, typeId);
        AssertDebug(subclassIndex >= 0, "Invalid subclass index");
        AssertDebug(subclassIndex < m_subclassImpls.Size(), "Invalid subclass index");

        if (!m_subclassImplsInitialized.Test(subclassIndex))
        {
            return nullptr;
        }

        return m_subclassImpls[subclassIndex].Get().GetElement(id);
    }

    void Advance(AdvanceAction action)
    {
        HYP_SCOPE;

        m_impl.Advance(action);

        for (Bitset::BitIndex i : m_subclassImplsInitialized)
        {
            m_subclassImpls[i].Get().Advance(action);
        }
    }

    void Reset()
    {
        m_impl.Reset();

        for (Bitset::BitIndex i : m_subclassImplsInitialized)
        {
            m_subclassImpls[i].Get().Reset();
        }
    }

    HYP_DEF_STL_BEGIN_END(Iterator(const_cast<ResourceTracker*>(this), Bitset::notFound, 0), Iterator(const_cast<ResourceTracker*>(this), Bitset::notFound, Bitset::notFound))

protected:
    struct Impl final
    {
        /*! \brief Checks if it already has a proxy for the given Id from the previous frame */
        HYP_FORCE_INLINE bool HasElement(IdType id) const
        {
            AssertDebug(id.GetTypeId() == typeId, "ResourceTracker typeid mismatch");

            return previous.Test(id.ToIndex());
        }

        HYP_FORCE_INLINE Bitset GetAdded() const
        {
            const SizeType newNumBits = MathUtil::Max(previous.NumBits(), next.NumBits());

            return Bitset(next).SetNumBits(newNumBits) & ~Bitset(previous).SetNumBits(newNumBits);
        }

        HYP_FORCE_INLINE Bitset GetRemoved() const
        {
            const SizeType newNumBits = MathUtil::Max(previous.NumBits(), next.NumBits());

            return Bitset(previous).SetNumBits(newNumBits) & ~Bitset(next).SetNumBits(newNumBits);
        }

        HYP_FORCE_INLINE const Bitset& GetChanged() const
        {
            return changed;
        }

        void Track(IdType id, const ElementType& value, const int* versionPtr = nullptr, bool allowDuplicatesInSameFrame = true)
        {
            ElementType* currentValuePtr = nullptr;
            int* currentVersionPtr = nullptr;

            ResourceTrackState trackState = ResourceTrackState::UNCHANGED;

            if (!allowDuplicatesInSameFrame)
            {
                AssertDebug(!next.Test(id.ToIndex()), "Element with ID: {} already marked to be added for this iteration!", id);
            }

            if (previous.Test(id.ToIndex()) || next.Test(id.ToIndex()))
            {
                AssertDebug(elements.HasIndex(id.ToIndex()));

                currentValuePtr = &elements.Get(id.ToIndex());
                currentVersionPtr = &versions.Get(id.ToIndex());

                AssertDebug(currentValuePtr != nullptr && currentVersionPtr != nullptr);
            }

            bool isDoublyAddedThisFrame = false;

            if (allowDuplicatesInSameFrame)
            {
                if (next.Test(id.ToIndex()))
                {
                    isDoublyAddedThisFrame = true;

                    // already added for this iteration. check if we can just return or if we need to update

                    if (value == *currentValuePtr && (versionPtr == nullptr || *versionPtr == *currentVersionPtr))
                    {
                        // same values, early out
                        return;
                    }
                }
            }

            if (currentValuePtr != nullptr)
            {
                // elements and versions must be kept in sync
                AssertDebug(currentVersionPtr != nullptr);

                // Advance if version has changed or if elements are not equal
                if (value != *currentValuePtr || (versionPtr != nullptr && *versionPtr != *currentVersionPtr))
                {
                    if (!isDoublyAddedThisFrame)
                    {
                        // Mark as changed if it is found in the previous iteration
                        changed.Set(id.ToIndex(), true);
                    }

                    // update value and version
                    *currentValuePtr = value;
                    *currentVersionPtr = versionPtr ? *versionPtr : 0;

                    trackState = ResourceTrackState::CHANGED_MODIFIED;
                }
                else
                {
                    trackState = ResourceTrackState::UNCHANGED;
                }
            }
            else
            {
                if (!isDoublyAddedThisFrame)
                {
                    // sanity check - if not in previous iteration, it must not be in the changed list
                    AssertDebug(!changed.Test(id.ToIndex()));
                }

                // use emplace to destruct / reconstruct current element
                elements.Set(id.ToIndex(), value);
                versions.Set(id.ToIndex(), versionPtr ? *versionPtr : 0);

                trackState = ResourceTrackState::CHANGED_ADDED;
            }

            next.Set(id.ToIndex(), true);
        }

        HYP_FORCE_INLINE bool MarkToKeep(IdType id)
        {
            if (!previous.Test(id.ToIndex()))
            {
                return false;
            }

            next.Set(id.ToIndex(), true);

            return true;
        }

        HYP_FORCE_INLINE void MarkToRemove(IdType id)
        {
            next.Set(id.ToIndex(), false);
        }

        template <class AllocatorType>
        void GetRemoved(Array<IdType, AllocatorType>& outIds, bool includeChanged) const
        {
            HYP_SCOPE;

            Bitset removedBits = GetRemoved();

            if (includeChanged)
            {
                removedBits |= GetChanged();
            }

            outIds.Reserve(removedBits.Count());

            SizeType firstSetBitIndex;

            while ((firstSetBitIndex = removedBits.FirstSetBitIndex()) != -1)
            {
                const IdType id = IdType(ObjIdBase { typeId, uint32(firstSetBitIndex + 1) });

                outIds.PushBack(id);

                removedBits.Set(firstSetBitIndex, false);
            }
        }

        template <class AllocatorType>
        void GetRemoved(Array<ElementType, AllocatorType>& out, bool includeChanged) const
        {
            HYP_SCOPE;

            Bitset removedBits = GetRemoved();

            if (includeChanged)
            {
                removedBits |= GetChanged();
            }

            out.Reserve(out.Size() + removedBits.Count());

            Bitset::BitIndex firstSetBitIndex;

            while ((firstSetBitIndex = removedBits.FirstSetBitIndex()) != Bitset::notFound)
            {
                const IdType id = IdType(ObjIdBase { typeId, uint32(firstSetBitIndex + 1) });

                const ElementType* elem = elements.TryGet(id.ToIndex());
                AssertDebug(elem != nullptr);

                out.PushBack(*elem);

                removedBits.Set(firstSetBitIndex, false);
            }
        }

        template <class AllocatorType>
        void GetRemoved(Array<ElementType*, AllocatorType>& out, bool includeChanged) const
        {
            HYP_SCOPE;

            Bitset removedBits = GetRemoved();

            if (includeChanged)
            {
                removedBits |= GetChanged();
            }

            out.Reserve(out.Size() + removedBits.Count());

            Bitset::BitIndex firstSetBitIndex;

            while ((firstSetBitIndex = removedBits.FirstSetBitIndex()) != Bitset::notFound)
            {
                const IdType id = IdType(ObjIdBase { typeId, uint32(firstSetBitIndex + 1) });

                const ElementType* elem = elements.TryGet(id.ToIndex());
                AssertDebug(elem != nullptr);

                out.PushBack(const_cast<ElementType*>(elem));

                removedBits.Set(firstSetBitIndex, false);
            }
        }

        template <class AllocatorType>
        void GetAdded(Array<ElementType, AllocatorType>& out, bool includeChanged) const
        {
            HYP_SCOPE;

            Bitset newlyAddedBits = GetAdded();

            if (includeChanged)
            {
                newlyAddedBits |= GetChanged();
            }

            out.Reserve(out.Size() + newlyAddedBits.Count());

            Bitset::BitIndex firstSetBitIndex;

            while ((firstSetBitIndex = newlyAddedBits.FirstSetBitIndex()) != Bitset::notFound)
            {
                const IdType id = IdType(ObjIdBase { typeId, uint32(firstSetBitIndex + 1) });

                const ElementType* elem = elements.TryGet(id.ToIndex());
                AssertDebug(elem != nullptr);

                out.PushBack(*elem);

                newlyAddedBits.Set(firstSetBitIndex, false);
            }
        }

        template <class AllocatorType>
        void GetAdded(Array<ElementType*, AllocatorType>& out, bool includeChanged) const
        {
            HYP_SCOPE;

            Bitset newlyAddedBits = GetAdded();

            if (includeChanged)
            {
                newlyAddedBits |= GetChanged();
            }

            out.Reserve(out.Size() + newlyAddedBits.Count());

            Bitset::BitIndex firstSetBitIndex;

            while ((firstSetBitIndex = newlyAddedBits.FirstSetBitIndex()) != Bitset::notFound)
            {
                const IdType id = IdType(ObjIdBase { typeId, uint32(firstSetBitIndex + 1) });

                const ElementType* elem = elements.TryGet(id.ToIndex());
                AssertDebug(elem != nullptr);

                out.PushBack(const_cast<ElementType*>(elem));

                newlyAddedBits.Set(firstSetBitIndex, false);
            }
        }

        template <class AllocatorType>
        void GetChanged(Array<IdType, AllocatorType>& outIds) const
        {
            HYP_SCOPE;

            Bitset changedBits = GetChanged();

            outIds.Reserve(changedBits.Count());

            SizeType firstSetBitIndex;

            while ((firstSetBitIndex = changedBits.FirstSetBitIndex()) != -1)
            {
                const IdType id = IdType(ObjIdBase { typeId, uint32(firstSetBitIndex + 1) });

                outIds.PushBack(id);

                changedBits.Set(firstSetBitIndex, false);
            }
        }

        template <class AllocatorType>
        void GetChanged(Array<ElementType, AllocatorType>& out) const
        {
            HYP_SCOPE;

            Bitset changedBits = GetChanged();

            out.Reserve(out.Size() + changedBits.Count());

            Bitset::BitIndex firstSetBitIndex;

            while ((firstSetBitIndex = changedBits.FirstSetBitIndex()) != Bitset::notFound)
            {
                const IdType id = IdType(ObjIdBase { typeId, uint32(firstSetBitIndex + 1) });

                const ElementType* elem = elements.TryGet(id.ToIndex());
                AssertDebug(elem != nullptr);

                out.PushBack(*elem);

                changedBits.Set(firstSetBitIndex, false);
            }
        }

        template <class AllocatorType>
        void GetChanged(Array<ElementType*, AllocatorType>& out) const
        {
            HYP_SCOPE;

            Bitset changedBits = GetChanged();

            out.Reserve(out.Size() + changedBits.Count());

            Bitset::BitIndex firstSetBitIndex;

            while ((firstSetBitIndex = changedBits.FirstSetBitIndex()) != Bitset::notFound)
            {
                const IdType id = IdType(ObjIdBase { typeId, uint32(firstSetBitIndex + 1) });

                const ElementType* elem = elements.TryGet(id.ToIndex());
                AssertDebug(elem != nullptr);

                out.PushBack(const_cast<ElementType*>(elem));

                changedBits.Set(firstSetBitIndex, false);
            }
        }

        template <class AllocatorType>
        void GetCurrent(Array<ElementType, AllocatorType>& out) const
        {
            HYP_SCOPE;

            Bitset currentBits = previous;

            out.Reserve(out.Size() + currentBits.Count());

            Bitset::BitIndex firstSetBitIndex;

            while ((firstSetBitIndex = currentBits.FirstSetBitIndex()) != Bitset::notFound)
            {
                const IdType id = IdType(ObjIdBase { typeId, uint32(firstSetBitIndex + 1) });

                const ElementType* elem = elements.TryGet(id.ToIndex());
                AssertDebug(elem != nullptr);

                out.PushBack(*elem);

                currentBits.Set(firstSetBitIndex, false);
            }
        }

        template <class AllocatorType>
        void GetCurrent(Array<ElementType*, AllocatorType>& out) const
        {
            HYP_SCOPE;

            Bitset currentBits = previous;

            out.Reserve(out.Size() + currentBits.Count());

            Bitset::BitIndex firstSetBitIndex;

            while ((firstSetBitIndex = currentBits.FirstSetBitIndex()) != Bitset::notFound)
            {
                const IdType id = IdType(ObjIdBase { typeId, uint32(firstSetBitIndex + 1) });

                const ElementType* elem = elements.TryGet(id.ToIndex());
                AssertDebug(elem != nullptr);

                out.PushBack(const_cast<ElementType*>(elem));

                currentBits.Set(firstSetBitIndex, false);
            }
        }

        ElementType* GetElement(IdType id)
        {
            AssertDebug(id.GetTypeId() == typeId);

            if (id.GetTypeId() != typeId)
            {
                return nullptr;
            }

            return elements.TryGet(id.ToIndex());
        }

        void Advance(AdvanceAction action)
        {
            HYP_SCOPE;

            { // Remove proxies for removed bits
                for (Bitset::BitIndex index : GetRemoved())
                {
                    AssertDebug(elements.HasIndex(index));

                    elements.EraseAt(index);
                    versions.EraseAt(index);
                }
            }

            switch (action)
            {
            case AdvanceAction::CLEAR:
                // Next state starts out zeroed out -- and next call to Advance will remove proxies for these objs
                previous = std::move(next);

                break;
            case AdvanceAction::PERSIST:
                previous = next;

                break;
            default:
                HYP_UNREACHABLE();

                break;
            }

            changed.Clear();
        }

        /*! \brief Total reset of the list, including clearing the previous state. */
        void Reset()
        {
            elements.Clear();
            versions.Clear();

            previous.Clear();
            next.Clear();
            changed.Clear();
        }

        TypeId typeId;

        ElementArrayType elements;
        // per-element version identifier array - mirrors elements array
        VersionArrayType versions;

        Bitset previous;
        Bitset next;
        Bitset changed;
    };

    // base class impl
    Impl m_impl;

    // per-subtype implementations (only constructed and setup on first Bind() call with that type)
    Array<ValueStorage<Impl>> m_subclassImpls;
    Bitset m_subclassImplsInitialized;
};

} // namespace hyperion

#endif