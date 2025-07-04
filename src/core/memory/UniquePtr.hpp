/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UNIQUE_PTR_HPP
#define HYPERION_UNIQUE_PTR_HPP

#include <core/Defines.hpp>

#include <core/utilities/TypeID.hpp>

#include <core/memory/Memory.hpp>
#include <core/memory/RefCountedPtr.hpp>
#include <core/memory/Any.hpp>

#include <core/object/HypObjectFwd.hpp>

#include <Types.hpp>
#include <Constants.hpp>

#include <cstdlib>

namespace hyperion {

class HypClass;

extern HYP_API const HypClass* GetClass(TypeID type_id);
extern HYP_API bool IsInstanceOfHypClass(const HypClass* hyp_class, const void* ptr, TypeID type_id);

namespace memory {

template <class T>
class UniquePtr;

struct UniquePtrHolder
{
    void* value;
    TypeID type_id;
    TypeID base_type_id;
    void (*dtor)(void*);

    UniquePtrHolder()
        : value(nullptr),
          type_id(TypeID::ForType<void>()),
          base_type_id(TypeID::ForType<void>()),
          dtor(nullptr)
    {
    }

    UniquePtrHolder(const UniquePtrHolder& other)
        : value(other.value),
          type_id(other.type_id),
          base_type_id(other.base_type_id),
          dtor(other.dtor)
    {
    }

    UniquePtrHolder& operator=(const UniquePtrHolder& other)
    {
        if (this == &other)
        {
            return *this;
        }

        value = other.value;
        type_id = other.type_id;
        base_type_id = other.base_type_id;
        dtor = other.dtor;

        return *this;
    }

    UniquePtrHolder(UniquePtrHolder&& other) noexcept
        : value(other.value),
          type_id(other.type_id),
          base_type_id(other.base_type_id),
          dtor(other.dtor)
    {
        other.value = nullptr;
        other.type_id = TypeID::ForType<void>();
        other.base_type_id = TypeID::ForType<void>();
        other.dtor = nullptr;
    }

    UniquePtrHolder& operator=(UniquePtrHolder&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        value = other.value;
        type_id = other.type_id;
        base_type_id = other.base_type_id;
        dtor = other.dtor;

        other.value = nullptr;
        other.type_id = TypeID::ForType<void>();
        other.base_type_id = TypeID::ForType<void>();
        other.dtor = nullptr;

        return *this;
    }

    template <class Base, class Derived, class... Args>
    void Construct(Args&&... args)
    {
        value = Memory::AllocateAndConstruct<Derived>(std::forward<Args>(args)...);
        dtor = &Memory::DestructAndFree<Derived>;
        type_id = TypeID::ForType<Derived>();
        base_type_id = TypeID::ForType<Base>();
    }

    template <class Base, class Derived>
    void TakeOwnership(Derived* ptr)
    {
        value = ptr;
        dtor = &Memory::DestructAndFree<Derived>;
        type_id = TypeID::ForType<Derived>();
        base_type_id = TypeID::ForType<Base>();
    }

    void Destruct()
    {
        dtor(value);
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return value != nullptr;
    }
};

class UniquePtrBase
{
public:
    UniquePtrBase() = default;

    UniquePtrBase(const UniquePtrBase& other) = delete;
    UniquePtrBase& operator=(const UniquePtrBase& other) = delete;

    UniquePtrBase(UniquePtrBase&& other) noexcept
        : m_holder(std::move(other.m_holder))
    {
    }

    UniquePtrBase& operator=(UniquePtrBase&& other) noexcept
    {
        Reset();

        m_holder = std::move(other.m_holder);

        return *this;
    }

    ~UniquePtrBase()
    {
        Reset();
    }

    HYP_FORCE_INLINE void* Get() const
    {
        return m_holder.value;
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return Get() != nullptr;
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return Get() == nullptr;
    }

    HYP_FORCE_INLINE bool operator==(const UniquePtrBase& other) const
    {
        return m_holder.value == other.m_holder.value;
    }

    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
    {
        return Get() == nullptr;
    }

    HYP_FORCE_INLINE bool operator!=(const UniquePtrBase& other) const
    {
        return m_holder.value != other.m_holder.value;
    }

    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
    {
        return Get() != nullptr;
    }

    HYP_FORCE_INLINE TypeID GetTypeID() const
    {
        return m_holder.type_id;
    }

    /*! \brief Destroys any currently held object.  */
    HYP_FORCE_INLINE void Reset()
    {
        if (m_holder)
        {
            m_holder.Destruct();
            m_holder = UniquePtrHolder();
        }
    }

    /*! \brief Releases the ptr to be managed externally.
        The value held within the UniquePtr will be unset,
        and the T* returned from this method will NEED to be deleted
        manually. */
    HYP_NODISCARD HYP_FORCE_INLINE void* Release()
    {
        if (m_holder)
        {
            void* ptr = m_holder.value;
            m_holder = UniquePtrHolder();

            return ptr;
        }

        return nullptr;
    }

    template <class T>
    HYP_FORCE_INLINE UniquePtr<T> CastUnsafe()
    {
        return UniquePtr<T>(static_cast<T*>(Release()));
    }

protected:
    explicit UniquePtrBase(UniquePtrHolder&& holder)
        : m_holder(std::move(holder))
    {
    }

    HYP_FORCE_INLINE TypeID GetBaseTypeID() const
    {
        return m_holder.base_type_id;
    }

    UniquePtrHolder m_holder;
};

/*! \brief A unique pointer with type erasure built in, so anything could be stored as UniquePtr<void>.
    You can also store a derived pointer in a UniquePtr<Base>, and it will be convertible back to UniquePtr<Derived>, as
    well as having the destructor called correctly without needing it to be virtual.
*/
template <class T>
class UniquePtr : public UniquePtrBase
{
protected:
    using Base = UniquePtrBase;

public:
    UniquePtr()
        : Base()
    {
    }

    UniquePtr(std::nullptr_t)
        : Base()
    {
    }

    /*! \brief Takes ownership of ptr.

        Ty may be a derived class of T, and the type ID of Ty will be stored, allowing
        for conversion back to UniquePtr<Ty> using Cast<Ty>().

        Do not delete the pointer passed to this,
        as it will be automatically deleted when this object or any object that takes ownership
        over from this object is destroyed. */

    template <class Ty>
    explicit UniquePtr(Ty* ptr)
        : Base()
    {
        using TyN = NormalizedType<Ty>;
        static_assert(std::is_convertible_v<std::add_pointer_t<TyN>, std::add_pointer_t<T>>, "Ty must be convertible to T!");

        Reset<Ty>(ptr);
    }

    UniquePtr(const UniquePtr& other) = delete;
    UniquePtr& operator=(const UniquePtr& other) = delete;

    /*! \brief Allows construction from a UniquePtr of a convertible type. */
    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    UniquePtr(UniquePtr<Ty>&& other) noexcept
        : Base()
    {
        Reset<Ty>(other.Release());
    }

    /*! \brief Allows assign from a UniquePtr of a convertible type. */
    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    UniquePtr& operator=(UniquePtr<Ty>&& other) noexcept
    {
        Reset<Ty>(other.Release());

        return *this;
    }

    UniquePtr(UniquePtr&& other) noexcept
        : Base(std::move(other))
    {
    }

    UniquePtr& operator=(UniquePtr&& other) noexcept
    {
        Base::operator=(std::move(other));

        return *this;
    }

    ~UniquePtr() = default;

    HYP_FORCE_INLINE T* Get() const
    {
        return static_cast<T*>(Base::m_holder.value);
    }

    template <class OtherType>
    HYP_FORCE_INLINE OtherType* TryGetAs() const
    {
        if (!Is<OtherType>())
        {
            return nullptr;
        }

        return static_cast<OtherType*>(Base::m_holder.value);
    }

    template <class OtherType>
    HYP_FORCE_INLINE OtherType* TryGetAsDynamic() const
    {
        if (OtherType* ptr = dynamic_cast<OtherType*>(Get()))
        {
            return ptr;
        }

        return nullptr;
    }

    HYP_FORCE_INLINE T* operator->() const
    {
        return static_cast<T*>(Base::m_holder.value);
    }

    HYP_FORCE_INLINE T& operator*() const&
    {
        return *static_cast<T*>(Base::m_holder.value);
    }

    HYP_FORCE_INLINE T operator*() &&
    {
        T result = std::move(*static_cast<T*>(Base::m_holder.value));
        Reset();

        return result;
    }

    HYP_FORCE_INLINE bool operator<(const UniquePtr& other) const
    {
        return uintptr_t(Base::Get()) < uintptr_t(other.Base::Get());
    }

    /*! \brief Drops any currently held valeu and constructs a new value using \ref{value}.

        Ty may be a derived class of T, and the type ID of Ty will be stored, allowing
        for conversion back to UniquePtr<Ty> using Cast<Ty>(). */
    template <class Ty>
    HYP_FORCE_INLINE void Set(Ty&& value)
    {
        using TyN = NormalizedType<Ty>;
        static_assert(std::is_convertible_v<std::add_pointer_t<TyN>, std::add_pointer_t<T>>, "Ty must be convertible to T!");

        Base::Reset();

        Base::m_holder.template Construct<T, TyN>(std::forward<Ty>(value));
    }

    /*! \brief Takes ownership of {ptr}, dropping the reference to the currently held value,
        if any.

        Ty may be a derived class of T, and the type ID of Ty will be stored, allowing
        for conversion back to UniquePtr<Ty> using Cast<Ty>().

        Note, do not delete the ptr after passing it to Reset(), as it will be deleted
        automatically. */
    template <class Ty>
    HYP_FORCE_INLINE void Reset(Ty* ptr)
    {
        using TyN = NormalizedType<Ty>;
        static_assert(std::is_convertible_v<std::add_pointer_t<TyN>, std::add_pointer_t<T>>, "Ty must be convertible to T!");

        Base::Reset();

        if (ptr)
        {
            Base::m_holder.template TakeOwnership<T, TyN>(ptr);
        }
    }

    HYP_FORCE_INLINE void Reset(std::nullptr_t)
    {
        Reset();
    }

    /*! \brief Destroys any currently held object.  */
    HYP_FORCE_INLINE void Reset()
    {
        Base::Reset();
    }

    /*! \brief Like Reset(), but constructs the object in-place. */
    template <class... Args>
    HYP_FORCE_INLINE UniquePtr& Emplace(Args&&... args)
    {
        return (*this = Construct(std::forward<Args>(args)...));
    }

    /*! \brief Like Emplace() but the first template parameter is specified as the type to construct. */
    template <class Ty, class... Args>
    HYP_FORCE_INLINE UniquePtr& EmplaceAs(Args&&... args)
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, "Ty must be convertible to T!");

        return (*this = MakeUnique<Ty>(std::forward<Args>(args)...));
    }

    /*! \brief Releases the ptr to be managed externally.
        The value held within the UniquePtr will be unset,
        and the T* returned from this method will NEED to be deleted
        manually. */
    HYP_NODISCARD HYP_FORCE_INLINE T* Release()
    {
        return static_cast<T*>(Base::Release());
    }

    /*! \brief Constructs a new RefCountedPtr from this object.
        The value held within this UniquePtr will be unset,
        the RefCountedPtr taking over management of the pointer. */
    template <class CountType = AtomicVar<uint32>>
    HYP_NODISCARD HYP_FORCE_INLINE RefCountedPtr<T, CountType> ToRefCountedPtr()
    {
        RefCountedPtr<T, CountType> rc;
        rc.Reset(Release());

        return rc;
    }

    /*! \brief Constructs a new UniquePtr<T> from the given arguments. */
    template <class... Args>
    HYP_NODISCARD HYP_FORCE_INLINE static UniquePtr Construct(Args&&... args)
    {
        static_assert(std::is_constructible_v<T, Args...>, "T must be constructible using the given args");

        UniquePtr ptr;

        if constexpr (IsHypObject<T>::value)
        {
            ptr.Reset(Memory::AllocateAndConstructWithContext<T, HypObjectInitializerGuard<T>>(std::forward<Args>(args)...));
        }
        else
        {
            ptr.Reset(Memory::AllocateAndConstruct<T>(std::forward<Args>(args)...));
        }

        return ptr;
    }

    /*! \brief Returns a boolean indicating whether the type of this UniquePtr is the same as the given type, or if the given type is a base class of the type of this UniquePtr.
     *  If T has a HypClass registered, this function will also return true if the held object is a subclass of T
     *
     *  \note This function will not check if the pointer can be casted to the given type using dynamically, so if you have a base class pointer and want to check if it can be casted to a derived class, use IsDynamic().
     *  However, there is still limited functionality for checking if a base class pointer can be casted to a derived class pointer, as long as the UniquePtr was constructed or Reset() with a derived class pointer. */
    template <class Ty>
    HYP_FORCE_INLINE bool Is() const
    {
        constexpr TypeID type_id = TypeID::ForType<Ty>();

        return std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>>
            || std::is_same_v<Ty, void>
            || GetTypeID() == type_id
            || GetBaseTypeID() == type_id
            || IsInstanceOfHypClass(GetClass(type_id), m_holder.value, GetTypeID());
    }

    /*! \brief Returns a boolean indicating whether the type of this UniquePtr is the same as the given type, or if the given type is a base class of the type of this UniquePtr.
     *  If T has a HypClass registered, this function will also return true if the held object is a subclass of T
     *  This function will also check if the pointer can be casted to the given type using dynamic_cast. */
    template <class Ty>
    HYP_FORCE_INLINE bool IsDynamic() const
    {
        return Is<Ty>() || dynamic_cast<Ty*>(Get()) != nullptr;
    }

    /*! \brief Attempts to cast the pointer directly to the given type.
        If the types are not compatible (Derived -> Base) or equal (or T is not void, in the case of a void pointer),
        no cast is performed and a null UniquePtr is returned. Otherwise, the
        value currently held in the UniquePtr being casted is std::move'd to the returned value. */
    template <class Ty>
    HYP_NODISCARD HYP_FORCE_INLINE UniquePtr<Ty> Cast()
    {
        if (Is<Ty>())
        {
            return Base::CastUnsafe<Ty>();
        }

        return UniquePtr<Ty>();
    }

    /*! \brief Attempts to cast the pointer to the given type using dynamic_cast.
        If the types are not compatible (Derived -> Base) or equal (or T is not void, in the case of a void pointer),
        no cast is performed and a null UniquePtr is returned. Otherwise, the
        value currently held in the UniquePtr being casted is std::move'd to the returned value. */
    template <class Ty>
    HYP_NODISCARD HYP_FORCE_INLINE UniquePtr<Ty> CastDynamic()
    {
        Ty* result = nullptr;
        if (Is<Ty>() || (result = dynamic_cast<Ty*>(Get())))
        {
            return result;
        }

        return UniquePtr<Ty>();
    }

    /*! \brief Conversion to base class UniquePtr. Performs a Cast(). */
    // template <class Ty, typename = typename std::enable_if_t<std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>>>>
    // operator UniquePtr<Ty>()
    //{
    //     return CastUnsafe<Ty>();
    // }
};

/*! \brief A UniquePtr<void> cannot be constructed except for being moved from an existing
    UniquePtr<T>. This is because UniquePtr<T> may hold a derived pointer, while still being convertible to
    the derived type. In order to maintain that functionality on UniquePtr<void>, we need to store the TypeID of T.
    We would not be able to do that with UniquePtr<void>.*/
// void pointer specialization
template <>
class UniquePtr<void> : public UniquePtrBase
{
protected:
    using Base = UniquePtrBase;

public:
    UniquePtr()
        : Base()
    {
    }

    explicit UniquePtr(Any&& value)
        : Base()
    {
        Base::m_holder.type_id = value.m_type_id;
        Base::m_holder.base_type_id = value.m_type_id;
        Base::m_holder.value = value.m_ptr;
        Base::m_holder.dtor = value.m_dtor;

        (void)value.Release<void>();
    }

    UniquePtr(const Base& other) = delete;
    UniquePtr& operator=(const Base& other) = delete;
    UniquePtr(const UniquePtr& other) = delete;
    UniquePtr& operator=(const UniquePtr& other) = delete;

    UniquePtr(Base&& other) noexcept
        : Base(std::move(other))
    {
    }

    UniquePtr& operator=(Base&& other) noexcept
    {
        Base::operator=(std::move(other));

        return *this;
    }

    UniquePtr(UniquePtr&& other) noexcept
        : Base(std::move(other))
    {
    }

    UniquePtr& operator=(UniquePtr&& other) noexcept
    {
        Base::operator=(std::move(other));

        return *this;
    }

    ~UniquePtr() = default;

    HYP_FORCE_INLINE void* Get() const
    {
        return Base::Get();
    }

    HYP_FORCE_INLINE bool operator<(const UniquePtr& other) const
    {
        return uintptr_t(Base::Get()) < uintptr_t(other.Base::Get());
    }

    /*! \brief Destroys any currently held object.  */
    HYP_FORCE_INLINE void Reset()
    {
        Base::Reset();
    }

    template <class Ty>
    HYP_FORCE_INLINE bool Is() const
    {
        constexpr TypeID type_id = TypeID::ForType<Ty>();

        return std::is_same_v<Ty, void>
            || GetTypeID() == type_id
            || GetBaseTypeID() == type_id
            || IsInstanceOfHypClass(GetClass(type_id), m_holder.value, GetTypeID());
    }

    /*! \brief Attempts to cast the pointer directly to the given type.
        If the types are not EXACT (or T is not void, in the case of a void pointer),
        no cast is performed and a null UniquePtr is returned. Otherwise, the
        value currently held in the UniquePtr being casted is moved to the returned value.

        \note Casting from UniquePtr<void> -> UniquePtr<T> will not be able to do any base class checking
        so you can only go from UniquePtr<void> to the exact specified type!

        \tparam Ty The type to cast to.
        \returns A UniquePtr<Ty> if the cast is successful, otherwise a null UniquePtr.
    */
    template <class Ty>
    HYP_NODISCARD HYP_FORCE_INLINE UniquePtr<Ty> Cast()
    {
        if (Is<Ty>())
        {
            return Base::CastUnsafe<Ty>();
        }

        return UniquePtr<Ty>();
    }

    /*! \brief Constructs a new RefCountedPtr from this object.
        The value held within this UniquePtr will be unset,
        the RefCountedPtr taking over management of the pointer.

        \returns A reference counted pointer to the value held in this UniquePtr.
    */
    template <class CountType = AtomicVar<uint32>>
    HYP_NODISCARD HYP_FORCE_INLINE RefCountedPtr<void, CountType> ToRefCountedPtr()
    {
        RefCountedPtr<void, CountType> rc;

        auto* ref_count_data = new typename RefCountedPtr<void, CountType>::RefCountedPtrBase::RefCountDataType;
        ref_count_data->InitFromParams(
            Base::m_holder.value,
            Base::m_holder.type_id,
            Base::m_holder.dtor);

        (void)Base::Release();

        // FIXME: For polymorphic types, will value point to the correct place?
        rc.SetRefCountData_Internal(Base::m_holder.value, ref_count_data, /* inc_ref */ true);

        return rc;
    }
};

template <class T>
struct MakeUniqueHelper
{
    template <class... Args>
    static UniquePtr<T> MakeUnique(Args&&... args)
    {
        return UniquePtr<T>::Construct(std::forward<Args>(args)...);
    }
};

} // namespace memory

template <class T>
using UniquePtr = memory::UniquePtr<T>;

using AnyPtr = UniquePtr<void>;

template <class T, class... Args>
HYP_FORCE_INLINE UniquePtr<T> MakeUnique(Args&&... args)
{
    return memory::MakeUniqueHelper<T>::MakeUnique(std::forward<Args>(args)...);
}

} // namespace hyperion

#endif