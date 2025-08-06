/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/memory/Memory.hpp>

#include <core/utilities/ValueStorage.hpp>

#include <core/Types.hpp>
#include <core/Constants.hpp>

#include <cstdlib>

namespace hyperion {
namespace memory {

/*! Hides implementation from outside observers using the PIMPL pattern (pointer to implementation
 *   Bit like a UniquePtr, but simpler and more adapted to the use case. */
template <class T>
class Pimpl
{
    struct AllocationBase
    {
        void (*destructObject)(void*);
    };

    struct Allocation : AllocationBase
    {
        ValueStorage<T> storage;
    };

public:
    Pimpl()
        : m_allocation(nullptr)
    {
    }

    Pimpl(std::nullptr_t)
        : m_allocation(nullptr)
    {
    }

    Pimpl(const Pimpl& other) = delete;
    Pimpl& operator=(const Pimpl& other) = delete;

    Pimpl(Pimpl&& other) noexcept
        : m_allocation(other.m_allocation)
    {
        other.m_allocation = nullptr;
    }

    Pimpl& operator=(Pimpl&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        if (m_allocation)
        {
            Reset();
        }

        m_allocation = other.m_allocation;

        other.m_allocation = nullptr;

        return *this;
    }

    ~Pimpl()
    {
        Reset();
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return m_allocation != nullptr;
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return m_allocation == nullptr;
    }

    HYP_FORCE_INLINE bool operator==(const Pimpl& other) const
    {
        return m_allocation == other.m_allocation;
    }

    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
    {
        return m_allocation == nullptr;
    }

    HYP_FORCE_INLINE bool operator!=(const Pimpl& other) const
    {
        return m_allocation != other.m_allocation;
    }

    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
    {
        return m_allocation != nullptr;
    }

    HYP_FORCE_INLINE T* Get() const
    {
        return m_allocation ? (static_cast<Allocation*>(m_allocation))->storage.GetPointer() : nullptr;
        // return HYP_ALIGN_PTR_AS(reinterpret_cast<uintptr_t>(m_allocation) + offsetof(Allocation, storage), T);
    }

    HYP_FORCE_INLINE T* operator->() const
    {
        return Get();
    }

    HYP_FORCE_INLINE T& operator*() const
    {
        return *Get();
    }

    HYP_FORCE_INLINE bool operator<(const Pimpl& other) const
    {
        return uintptr_t(m_allocation) < uintptr_t(other.m_allocation);
    }

    HYP_FORCE_INLINE void Reset(std::nullptr_t)
    {
        Reset();
    }

    /*! \brief Destroys any currently held object.  */
    HYP_FORCE_INLINE void Reset()
    {
        if (m_allocation)
        {
            m_allocation->destructObject(m_allocation);

            Memory::Free(m_allocation);
            m_allocation = nullptr;
        }
    }

    /*! \brief Like Reset(), but constructs the object in-place. */
    template <class... Args>
    HYP_FORCE_INLINE Pimpl& Emplace(Args&&... args)
    {
        return (*this = Construct(std::forward<Args>(args)...));
    }

    /*! \brief Like Emplace() but the first template parameter is specified as the type to construct. */
    template <class Ty, class... Args>
    HYP_FORCE_INLINE Pimpl& EmplaceAs(Args&&... args)
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, "Ty must be convertible to T!");

        return (*this = MakePimpl<Ty>(std::forward<Args>(args)...));
    }

    /*! \brief Constructs a Pimpl<T> from the given arguments. */
    template <class... Args>
    HYP_NODISCARD HYP_FORCE_INLINE static Pimpl Construct(Args&&... args)
    {
        static_assert(std::is_constructible_v<T, Args...>, "T must be constructible using the given args");
        static_assert(std::is_trivial_v<Allocation>, "Allocation type must be trivial");

        Pimpl pimpl;

        Allocation* allocation = Memory::Allocate<Allocation>();
        allocation->destructObject = [](void* allocation)
        {
            static_cast<Allocation*>(allocation)->storage.Destruct();
        };
        allocation->storage.Construct(std::forward<Args>(args)...);

        pimpl.m_allocation = allocation;

        return pimpl;
    }

private:
    AllocationBase* m_allocation;
};

template <class T>
struct MakePimplHelper
{
    template <class... Args>
    static Pimpl<T> MakePimpl(Args&&... args)
    {
        return Pimpl<T>::Construct(std::forward<Args>(args)...);
    }
};

} // namespace memory

template <class T>
using Pimpl = memory::Pimpl<T>;

template <class T, class... Args>
HYP_FORCE_INLINE Pimpl<T> MakePimpl(Args&&... args)
{
    return memory::MakePimplHelper<T>::MakePimpl(std::forward<Args>(args)...);
}

} // namespace hyperion
