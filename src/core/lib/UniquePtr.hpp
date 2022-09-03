#ifndef HYPERION_V2_LIB_UNIQUE_PTR_HPP
#define HYPERION_V2_LIB_UNIQUE_PTR_HPP

#include <util/Defines.hpp>
#include <core/lib/TypeID.hpp>
#include <core/lib/CMemory.hpp>
#include <core/lib/RefCountedPtr.hpp>
#include <Types.hpp>
#include <Constants.hpp>

#include <atomic>
#include <cstdlib>

namespace hyperion {
namespace detail {

template <class T>
class UniquePtr;

struct UniquePtrHolder
{
    void *value;
    TypeID type_id;
    void(*dtor)(void *);

    template <class T, class ...Args>
    void Construct(Args &&... args)
    {
        using Normalized = NormalizedType<T>;

        value = Memory::AllocateAndConstruct<Normalized>(std::forward<Args>(args)...);
        dtor = &Memory::DestructAndFree<Normalized>;
        type_id = TypeID::ForType<Normalized>();
    }

    template <class T>
    void TakeOwnership(T *ptr)
    {
        using Normalized = NormalizedType<T>;

        value = ptr;
        dtor = &Memory::DestructAndFree<Normalized>;
        type_id = TypeID::ForType<T>();
    }

    void Destruct()
    {
        dtor(value);
        type_id = TypeID::ForType<void>();
        value = nullptr;
        dtor = nullptr;
    }
};

class UniquePtrBase
{
public:
    UniquePtrBase()
        : m_ref(nullptr)
    {
    }

    UniquePtrBase(const UniquePtrBase &other) = delete;
    UniquePtrBase &operator=(const UniquePtrBase &other) = delete;

    UniquePtrBase(UniquePtrBase &&other) noexcept
        : m_ref(other.m_ref)
    {
        other.m_ref = nullptr;
    }

    UniquePtrBase &operator=(UniquePtrBase &&other) noexcept
    {
        Reset();

        m_ref = other.m_ref;
        other.m_ref = nullptr;

        return *this;
    }

    ~UniquePtrBase()
    {
        Reset();
    }

    [[nodiscard]] HYP_FORCE_INLINE void *Get() const
        { return m_ref ? m_ref->value : nullptr; }

    explicit operator bool() const
        { return Get() != nullptr; }

    bool operator!() const
        { return Get() == nullptr; }

    bool operator==(const UniquePtrBase &other) const
        { return m_ref == other.m_ref; }

    bool operator==(std::nullptr_t) const
        { return Get() == nullptr; }

    bool operator!=(const UniquePtrBase &other) const
        { return m_ref != other.m_ref; }

    bool operator!=(std::nullptr_t) const
        { return Get() != nullptr; }

    [[nodiscard]] const TypeID &GetTypeID() const
        { return m_ref ? m_ref->type_id : TypeID::ForType<void>(); }

    /*! \brief Attempts to cast the pointer directly to the given type.
        If the types are not exact, no cast is performed and a null pointer
        UniquePtr is returned. Otherwise, the value currently held in the UniquePtr
        being casted is std::move'd to the returned value. */
    template <class T>
    [[nodiscard]] HYP_FORCE_INLINE UniquePtr<T> Cast()
    {
        if (GetTypeID() == TypeID::ForType<T>()) {
            return CastUnsafe<T>();
        }

        return UniquePtr<T>();
    }

    /*! \brief Destroys any currently held object.  */
    HYP_FORCE_INLINE void Reset()
    {
        if (m_ref) {
            m_ref->Destruct();
            delete m_ref;
            m_ref = nullptr;
        }
    }

    /*! \brief Releases the ptr to be managed externally.
        The value held within the UniquePtr will be unset,
        and the T* returned from this method will NEED to be deleted
        manually. */
    [[nodiscard]] HYP_FORCE_INLINE void *Release()
    {
        if (!m_ref) {
            return nullptr;
        }

        void *ptr = m_ref->value;

        // not calling m_ref->Destruct() because we don't
        // need to invoke dtor and we _definitely_ don't want
        // to invoke it while m_ref->value is still set.
        delete m_ref;
        m_ref = nullptr;

        return ptr;
    }

protected:
    explicit UniquePtrBase(UniquePtrHolder *holder)
        : m_ref(holder)
    {
    }

    template <class T>
    UniquePtr<T> CastUnsafe()
    {
        UniquePtr<T> unique;
        unique.m_ref = m_ref;
        m_ref = nullptr;

        return unique;
    }

    UniquePtrHolder *m_ref;
};

/*! \brief A unique pointer with type erasure built in */
template <class T>
class UniquePtr : public UniquePtrBase
{
protected:
    using Base = UniquePtrBase;

public:
    using Base::operator==;
    using Base::operator!=;
    using Base::operator!;
    using Base::operator bool;

    UniquePtr()
        : Base()
    {
    }

    /*! \brief Takes ownership of ptr. Do not delete the pointer passed to this,
        as it will be automatically deleted when this object or any object that takes ownership
        over from this object is destroyed. */
    explicit UniquePtr(T *ptr)
        : Base()
    {
        Reset(ptr);
    }

    explicit UniquePtr(const T &value)
        : Base(new UniquePtrHolder)
    {
        Base::m_ref->template Construct<T>(value);
    }

    explicit UniquePtr(T &&value)
        : Base(new UniquePtrHolder)
    {
        Base::m_ref->template Construct<T>(std::move(value));
    }

    UniquePtr(const UniquePtr &other) = delete;
    UniquePtr &operator=(const UniquePtr &other) = delete;

    UniquePtr(UniquePtr &&other) noexcept
        : Base(std::move(other))
    {
    }

    UniquePtr &operator=(UniquePtr &&other) noexcept
    {
        Base::operator=(std::move(other));

        return *this;
    }

    ~UniquePtr() = default;

    HYP_FORCE_INLINE T *Get() const
        { return Base::m_ref ? static_cast<T *>(Base::m_ref->value) : nullptr; }

    HYP_FORCE_INLINE T *operator->() const
        { return Get(); }

    HYP_FORCE_INLINE T &operator*()
        { return *Get(); }

    HYP_FORCE_INLINE const T &operator*() const
        { return *Get(); }

    void Set(const T &value)
    {
        Base::Reset();

        Base::m_ref = new UniquePtrHolder;
        Base::m_ref->template Construct<T>(value);
    }

    void Set(T &&value)
    {
        Base::Reset();

        Base::m_ref = new UniquePtrHolder;
        Base::m_ref->template Construct<T>(std::move(value));
    }

    /*! \brief Takes ownership of {ptr}, dropping the reference to the currently held value,
        if any. Note, do not delete the ptr after passing it to Reset(), as it will be deleted
        automatically. */
    void Reset(T *ptr)
    {
        Base::Reset();

        if (ptr) {
            Base::m_ref = new UniquePtrHolder;
            Base::m_ref->template TakeOwnership<T>(ptr);
        }
    }

    /*! \brief Releases the ptr to be managed externally.
        The value held within the UniquePtr will be unset,
        and the T* returned from this method will NEED to be deleted
        manually. */
    [[nodiscard]] T *Release()
        { return static_cast<T *>(Base::Release()); }

    /*! \brief Constructs a new RefCountedPtr from this object.
        The value held within this UniquePtr will be unset,
        the RefCountedPtr taking over management of the pointer. */
    template <class CountType = UInt>
    [[nodiscard]] RefCountedPtr<T, CountType> MakeRefCounted()
    {
        RefCountedPtr<T, CountType> rc;
        rc.Reset(Release());

        return rc;
    }
};

// void pointer specialization -- just uses base class, but with Set() and Reset()
template <>
class UniquePtr<void> : public UniquePtrBase
{
protected:
    using Base = UniquePtrBase;

public:
    using Base::operator==;
    using Base::operator!=;
    using Base::operator!;
    using Base::operator bool;

    UniquePtr()
        : Base()
    {
    }

    UniquePtr(const Base &other) = delete;
    UniquePtr &operator=(const Base &other) = delete;

    UniquePtr(Base &&other) noexcept
        : Base(std::move(other))
    {
    }

    UniquePtr &operator=(Base &&other) noexcept
    {
        Base::operator=(std::move(other));

        return *this;
    }

    ~UniquePtr() = default;

    HYP_FORCE_INLINE void *Get() const
        { return Base::Get(); }

    template <class T>
    void Set(const T &value)
    {
        Base::Reset();

        Base::m_ref = new UniquePtrHolder;
        Base::m_ref->template Construct<T>(value);
    }

    template <class T>
    void Set(T &&value)
    {
        Base::Reset();

        Base::m_ref = new UniquePtrHolder;
        Base::m_ref->template Construct<T>(std::move(value));
    }

    /*! \brief Takes ownership of {ptr}, dropping the reference to the currently held value,
        if any. Note, do not delete the ptr after passing it to Reset(), as it will be deleted
        automatically. */
    template <class T>
    void Reset(T *ptr)
    {
        Base::Reset();

        if (ptr) {
            Base::m_ref = new UniquePtrHolder;
            Base::m_ref->template TakeOwnership<T>(ptr);
        }
    }

    /*! \brief Constructs a new RefCountedPtr from this object.
        The value held within this UniquePtr will be unset,
        the RefCountedPtr taking over management of the pointer. */
    template <class CountType = UInt>
    [[nodiscard]] RefCountedPtr<void, CountType> MakeRefCounted()
    {
        RefCountedPtr<void, CountType> rc;
        rc.Reset(Base::Release());

        return rc;
    }
};

} // namespace detail

template <class T>
using UniquePtr = detail::UniquePtr<T>;

} // namespace hyperion

#endif