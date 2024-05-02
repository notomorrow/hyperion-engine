/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_SAFE_DELETER_HPP

#define HYPERION_SAFE_DELETER_HPP

#include <core/Core.hpp>
#include <core/Containers.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Mutex.hpp>
#include <core/ID.hpp>

#include <rendering/Texture.hpp>
#include <rendering/Bindless.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RenderObject.hpp>

namespace hyperion {

using renderer::Device;
using renderer::GPUImageMemory;

class DeletionEntryBase
{
public:
    DeletionEntryBase()                                                 = default;
    DeletionEntryBase(const DeletionEntryBase &other)                   = delete;
    DeletionEntryBase &operator=(const DeletionEntryBase &other)        = delete;
    DeletionEntryBase(DeletionEntryBase &&other) noexcept               = delete;
    DeletionEntryBase &operator=(DeletionEntryBase &&other) noexcept    = delete;
    virtual ~DeletionEntryBase()                                        = default;

    /*! \brief Decrement the cycle count
     *  \return true if the cycle is zero, false otherwise */
    HYP_API bool DecrementCycle();

    /*! \brief Perform deletion of the object if the cycle is zero,
     *  otherwise decrement the cycle */
    HYP_API virtual bool PerformDeletion(bool force = false) final;

protected:
    virtual void PerformDeletionImpl() = 0;

    uint8   m_cycles_remaining = 0;
};

template <class Derived>
class DeletionEntry : public DeletionEntryBase
{
public:
    DeletionEntry()                                                 = default;
    DeletionEntry(const DeletionEntry &other)                       = delete;
    DeletionEntry &operator=(const DeletionEntry &other)            = delete;
    DeletionEntry(DeletionEntry &&other) noexcept                   = delete;
    DeletionEntry &operator=(DeletionEntry &&other) noexcept        = delete;
    virtual ~DeletionEntry() override                               = default;

protected:
    virtual void PerformDeletionImpl() override = 0;
};

template <class T>
class DeletionEntry<Handle<T>> : public DeletionEntryBase
{
public:
    DeletionEntry(Handle<T> &&handle)
        : m_handle(std::move(handle))
    {
    }

    DeletionEntry(const DeletionEntry &other)               = delete;
    DeletionEntry &operator=(const DeletionEntry &other)    = delete;

    DeletionEntry(DeletionEntry &&other) noexcept
        : m_handle(std::move(other.m_handle))
    {
    }

    DeletionEntry &operator=(DeletionEntry &&other) noexcept
    {
        m_handle = std::move(other.m_handle);

        return *this;
    }

    virtual ~DeletionEntry() override = default;

private:
    virtual void PerformDeletionImpl() override
    {
        m_handle.Reset();
    }

    Handle<T>   m_handle;
};

template <class T>
class DeletionEntry<renderer::RenderObjectHandle_Strong<T, renderer::Platform::CURRENT>> : public DeletionEntryBase
{
public:
    DeletionEntry(renderer::RenderObjectHandle_Strong<T, renderer::Platform::CURRENT> &&handle)
        : m_handle(std::move(handle))
    {
    }

    DeletionEntry(const DeletionEntry &other)               = delete;
    DeletionEntry &operator=(const DeletionEntry &other)    = delete;

    DeletionEntry(DeletionEntry &&other) noexcept
        : m_handle(std::move(other.m_handle))
    {
    }

    DeletionEntry &operator=(DeletionEntry &&other) noexcept
    {
        m_handle = std::move(other.m_handle);

        return *this;
    }

    virtual ~DeletionEntry() override = default;

private:
    virtual void PerformDeletionImpl() override
    {
        m_handle.Reset();
    }

    renderer::RenderObjectHandle_Strong<T, renderer::Platform::CURRENT> m_handle;
};

class SafeDeleter
{

public:
    static constexpr uint8 initial_cycles_remaining = uint8(max_frames_in_flight + 1);

    template <class T>
    void SafeRelease(T &&resource)
    {
        Mutex::Guard guard(m_render_resource_deletion_mutex);
        
        m_deletion_entries.PushBack(UniquePtr<DeletionEntry<T>>::Construct(std::move(resource)));

        m_render_resource_deletion_flag.Set(true, MemoryOrder::RELEASE);
    }

    void SafeRelease(RenderProxy &&proxy)
    {
        SafeRelease(std::move(proxy.mesh));
        SafeRelease(std::move(proxy.material));
        SafeRelease(std::move(proxy.skeleton));
    }

    void PerformEnqueuedDeletions();
    void ForceReleaseAll();

    bool CollectAllEnqueuedItems(Array<UniquePtr<DeletionEntryBase>> &out_entries);

private:
    Mutex                               m_render_resource_deletion_mutex;
    AtomicVar<bool>                     m_render_resource_deletion_flag = false;

    Array<UniquePtr<DeletionEntryBase>> m_deletion_entries;
};

} // namespace hyperion

#endif