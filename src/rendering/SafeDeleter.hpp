/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/LinkedList.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Mutex.hpp>
#include <core/object/ObjId.hpp>

#include <rendering/Bindless.hpp>
#include <rendering/RenderProxy.hpp>

#include <rendering/RenderDevice.hpp>
#include <rendering/RenderGpuBuffer.hpp>
#include <rendering/RenderObject.hpp>

namespace hyperion {

class DeletionEntryBase
{
public:
    DeletionEntryBase() = default;
    DeletionEntryBase(const DeletionEntryBase& other) = delete;
    DeletionEntryBase& operator=(const DeletionEntryBase& other) = delete;
    DeletionEntryBase(DeletionEntryBase&& other) noexcept = delete;
    DeletionEntryBase& operator=(DeletionEntryBase&& other) noexcept = delete;
    virtual ~DeletionEntryBase() = default;

    /*! \brief Decrement the cycle count
     *  \return true if the cycle is zero, false otherwise */
    HYP_API bool DecrementCycle();

    /*! \brief Perform deletion of the object if the cycle is zero,
     *  otherwise decrement the cycle */
    HYP_API virtual bool PerformDeletion(bool force = false) final;

protected:
    virtual void PerformDeletion_Internal() = 0;

    uint8 m_cyclesRemaining = 0;
};

template <class Derived>
class DeletionEntry;

template <class T>
class DeletionEntry<Handle<T>> : public DeletionEntryBase
{
public:
    DeletionEntry(Handle<T>&& handle)
        : m_handle(std::move(handle))
    {
    }

    DeletionEntry(const DeletionEntry& other) = delete;
    DeletionEntry& operator=(const DeletionEntry& other) = delete;

    DeletionEntry(DeletionEntry&& other) noexcept
        : m_handle(std::move(other.m_handle))
    {
    }

    DeletionEntry& operator=(DeletionEntry&& other) noexcept
    {
        m_handle = std::move(other.m_handle);

        return *this;
    }

    virtual ~DeletionEntry() override = default;

private:
    virtual void PerformDeletion_Internal() override
    {
        m_handle.Reset();
    }

    Handle<T> m_handle;
};

template <>
class DeletionEntry<AnyHandle> : public DeletionEntryBase
{
public:
    DeletionEntry(AnyHandle&& handle)
        : m_handle(std::move(handle))
    {
    }

    DeletionEntry(const DeletionEntry& other) = delete;
    DeletionEntry& operator=(const DeletionEntry& other) = delete;

    DeletionEntry(DeletionEntry&& other) noexcept
        : m_handle(std::move(other.m_handle))
    {
    }

    DeletionEntry& operator=(DeletionEntry&& other) noexcept
    {
        m_handle = std::move(other.m_handle);

        return *this;
    }

    virtual ~DeletionEntry() override = default;

private:
    virtual void PerformDeletion_Internal() override
    {
        m_handle.Reset();
    }

    AnyHandle m_handle;
};

template <class T>
class DeletionEntry<RenderObjectHandle_Strong<T>> : public DeletionEntryBase
{
public:
    DeletionEntry(RenderObjectHandle_Strong<T>&& handle)
        : m_handle(std::move(handle))
    {
    }

    DeletionEntry(const DeletionEntry& other) = delete;
    DeletionEntry& operator=(const DeletionEntry& other) = delete;

    DeletionEntry(DeletionEntry&& other) noexcept
        : m_handle(std::move(other.m_handle))
    {
    }

    DeletionEntry& operator=(DeletionEntry&& other) noexcept
    {
        m_handle = std::move(other.m_handle);

        return *this;
    }

    virtual ~DeletionEntry() override = default;

private:
    virtual void PerformDeletion_Internal() override
    {
        m_handle.Reset();
    }

    RenderObjectHandle_Strong<T> m_handle;
};

class SafeDeleter
{

public:
    static constexpr uint8 initialCyclesRemaining = uint8(g_framesInFlight + 1);

    template <class T>
    void SafeRelease(T&& resource)
    {
        Mutex::Guard guard(m_mutex);

        m_deletionEntries.PushBack(MakeUnique<DeletionEntry<T>>(std::move(resource)));

        m_numDeletionEntries.Increment(1, MemoryOrder::RELEASE);
    }

    template <class T>
    void SafeRelease(Array<T>&& resources)
    {
        Mutex::Guard guard(m_mutex);

        for (T& resource : resources)
        {
            m_deletionEntries.PushBack(MakeUnique<DeletionEntry<T>>(std::move(resource)));

            m_numDeletionEntries.Increment(1, MemoryOrder::RELEASE);
        }
    }

    void SafeRelease(RenderProxyMesh&& proxy)
    {
        SafeRelease(std::move(proxy.mesh));
        SafeRelease(std::move(proxy.material));
        SafeRelease(std::move(proxy.skeleton));
    }

    void PerformEnqueuedDeletions();
    void ForceDeleteAll();

    HYP_FORCE_INLINE int32 NumEnqueuedDeletions() const
    {
        return m_numDeletionEntries.Get(MemoryOrder::ACQUIRE);
    }

private:
    bool CollectAllEnqueuedItems(Array<UniquePtr<DeletionEntryBase>>& outEntries);

    Mutex m_mutex;
    AtomicVar<int32> m_numDeletionEntries { 0 };

    LinkedList<UniquePtr<DeletionEntryBase>> m_deletionEntries;
};

} // namespace hyperion
