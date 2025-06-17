/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_OPTIONAL_HPP
#define HYPERION_OPTIONAL_HPP

#include <core/utilities/ValueStorage.hpp>
#include <core/debug/Debug.hpp>

#include <Types.hpp>
#include <HashCode.hpp>

namespace hyperion {
namespace utilities {

template <class T, typename IsReferenceType = void>
class Optional;

template <class T>
class Optional<T, std::enable_if_t<!std::is_reference_v<std::remove_const_t<T>>>>
{
public:
    Optional()
        : m_hasValue(false)
    {
    }

    /*! \brief Constructs an Optional<T> from a pointer to T. If the given value is
     *  nullptr, it will be an empty Optional<T>. Otherwise, the value will be set to
     *  the value that is pointed to. */
    Optional(T* ptr)
        : m_hasValue(ptr != nullptr)
    {
        if (ptr != nullptr)
        {
            m_storage.Construct(*ptr);
        }
    }

    template <class Ty, class = std::enable_if_t<std::is_convertible_v<Ty, T>>>
    Optional(Ty&& value) noexcept
        : m_hasValue(true)
    {
        m_storage.Construct(std::forward<Ty>(value));
    }

    template <class Ty, class = std::enable_if_t<std::is_convertible_v<Ty, T>>>
    Optional& operator=(Ty&& value) noexcept
    {
        Set(std::forward<Ty>(value));

        return *this;
    }

    Optional(const Optional& other)
        : m_hasValue(other.m_hasValue)
    {
        if (m_hasValue)
        {
            m_storage.Construct(other.Get());
        }
    }

    Optional& operator=(const Optional& other)
    {
        if (&other == this)
        {
            return *this;
        }

        if (other.m_hasValue)
        {
            Set(other.Get());
        }
        else
        {
            Unset();
        }

        return *this;
    }

    Optional(Optional&& other) noexcept
        : m_hasValue(other.m_hasValue)
    {
        if (other.m_hasValue)
        {
            m_storage.Construct(std::move(other.Get()));

            other.m_storage.Destruct();
            other.m_hasValue = false;
        }
    }

    Optional& operator=(Optional&& other) noexcept
    {
        if (other.m_hasValue)
        {
            Set(std::move(other.Get()));

            other.m_storage.Destruct();
            other.m_hasValue = false;
        }
        else
        {
            Unset();
        }

        return *this;
    }

    ~Optional()
    {
        if (m_hasValue)
        {
            m_storage.Destruct();
        }
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return m_hasValue;
    }

    HYP_FORCE_INLINE bool operator==(const Optional& other) const
    {
        if (m_hasValue != other.m_hasValue)
        {
            return false;
        }

        if (m_hasValue)
        {
            return Get() == other.Get();
        }

        return true;
    }

    HYP_FORCE_INLINE bool operator!=(const Optional& other) const
    {
        if (m_hasValue != other.m_hasValue)
        {
            return true;
        }

        if (m_hasValue)
        {
            return Get() != other.Get();
        }

        return false;
    }

    HYP_FORCE_INLINE bool operator==(const T& value) const
    {
        if (!m_hasValue)
        {
            return false;
        }

        return Get() == value;
    }

    HYP_FORCE_INLINE bool operator!=(const T& value) const
    {
        if (!m_hasValue)
        {
            return true;
        }

        return Get() != value;
    }

    HYP_FORCE_INLINE T* TryGet()
    {
        if (m_hasValue)
        {
            return &Get();
        }

        return nullptr;
    }

    HYP_FORCE_INLINE const T* TryGet() const
    {
        if (m_hasValue)
        {
            return &Get();
        }

        return nullptr;
    }

    HYP_FORCE_INLINE T& Get() &
    {
        AssertThrow(m_hasValue);

        return m_storage.Get();
    }

    HYP_FORCE_INLINE const T& Get() const&
    {
        AssertThrow(m_hasValue);

        return m_storage.Get();
    }

    HYP_FORCE_INLINE T Get() &&
    {
        AssertThrow(m_hasValue);

        return std::move(m_storage.Get());
    }

    HYP_FORCE_INLINE const T& GetOr(const T& defaultValue) const&
    {
        if (m_hasValue)
        {
            return Get();
        }

        return defaultValue;
    }

    HYP_FORCE_INLINE T GetOr(T&& defaultValue) const&
    {
        if (m_hasValue)
        {
            return Get();
        }

        return std::move(defaultValue);
    }

    HYP_FORCE_INLINE T GetOr(T&& defaultValue) &&
    {
        if (m_hasValue)
        {
            return std::move(Get());
        }

        return std::move(defaultValue);
    }

    template <class Function, typename = std::enable_if_t<std::is_invocable_v<NormalizedType<Function>>>>
    HYP_FORCE_INLINE const T& GetOr(Function&& func) const&
    {
        if (m_hasValue)
        {
            return Get();
        }

        return func();
    }

    template <class Function, typename = std::enable_if_t<std::is_invocable_v<NormalizedType<Function>>>>
    HYP_FORCE_INLINE T GetOr(Function&& func) &&
    {
        if (m_hasValue)
        {
            return std::move(Get());
        }

        return func();
    }

    HYP_FORCE_INLINE void Set(const T& value)
    {
        if (m_hasValue)
        {
            m_storage.Destruct();
        }

        m_storage.Construct(value);
        m_hasValue = true;
    }

    HYP_FORCE_INLINE void Set(T&& value)
    {
        if (m_hasValue)
        {
            m_storage.Destruct();
        }

        m_storage.Construct(std::move(value));
        m_hasValue = true;
    }

    //! \brief Remove the held value, setting the Optional<> to a default state.
    HYP_FORCE_INLINE void Unset()
    {
        if (m_hasValue)
        {
            m_storage.Destruct();
            m_hasValue = false;
        }
    }

    /*! \brief Construct the held value in-place. */
    template <class... Args>
    HYP_FORCE_INLINE void Emplace(Args&&... args)
    {
        if (m_hasValue)
        {
            m_storage.Destruct();
        }

        m_storage.Construct(std::forward<Args>(args)...);
        m_hasValue = true;
    }

    HYP_FORCE_INLINE T* operator->()
    {
        return static_cast<T*>(m_storage.GetPointer());
    }

    HYP_FORCE_INLINE const T* operator->() const
    {
        return static_cast<const T*>(m_storage.GetPointer());
    }

    HYP_FORCE_INLINE T& operator*()
    {
        return Get();
    }

    HYP_FORCE_INLINE const T& operator*() const
    {
        return Get();
    }

    HYP_FORCE_INLINE bool HasValue() const
    {
        return m_hasValue;
    }

    HYP_FORCE_INLINE bool Any() const
    {
        return m_hasValue;
    }

    HYP_FORCE_INLINE bool Empty() const
    {
        return !m_hasValue;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        if (m_hasValue)
        {
            return HashCode::GetHashCode(Get());
        }

        return HashCode();
    }

private:
    ValueStorage<T> m_storage;

    bool m_hasValue;
};

template <class T>
class Optional<T, std::enable_if_t<std::is_reference_v<std::remove_const_t<T>>>>
{
public:
    Optional()
        : m_ptr(nullptr)
    {
    }

    /*! \brief Constructs an Optional<T> from a pointer to T. If the given value is
     *  nullptr, it will be an empty Optional<T>. Otherwise, the value will be set to
     *  the value that is pointed to. */
    Optional(typename std::remove_reference_t<T>* ptr)
        : m_ptr(ptr)
    {
    }

    Optional(typename std::remove_reference_t<T>& ref)
        : m_ptr(&ref)
    {
    }

    Optional& operator=(typename std::remove_reference_t<T>& ref)
    {
        m_ptr = &ref;

        return *this;
    }

    Optional(const Optional& other)
        : m_ptr(other.m_ptr)
    {
    }

    Optional& operator=(const Optional& other)
    {
        if (&other == this)
        {
            return *this;
        }

        m_ptr = other.m_ptr;

        return *this;
    }

    Optional(Optional&& other) noexcept
        : m_ptr(other.m_ptr)
    {
        other.m_ptr = nullptr;
    }

    Optional& operator=(Optional&& other) noexcept
    {
        if (&other == this)
        {
            return *this;
        }

        m_ptr = other.m_ptr;
        other.m_ptr = nullptr;

        return *this;
    }

    ~Optional() = default;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return m_ptr != nullptr;
    }

    HYP_FORCE_INLINE bool operator==(const Optional& other) const
    {
        return m_ptr == other.m_ptr;
    }

    HYP_FORCE_INLINE bool operator!=(const Optional& other) const
    {
        return m_ptr != other.m_ptr;
    }

    HYP_FORCE_INLINE bool operator==(const T& value) const
    {
        if (m_ptr == nullptr)
        {
            return false;
        }

        return *m_ptr == value;
    }

    HYP_FORCE_INLINE bool operator!=(const T& value) const
    {
        if (m_ptr == nullptr)
        {
            return true;
        }

        return *m_ptr != value;
    }

    HYP_FORCE_INLINE typename std::remove_reference_t<T>* TryGet()
    {
        return m_ptr;
    }

    HYP_FORCE_INLINE const typename std::remove_reference_t<T>* TryGet() const
    {
        return m_ptr;
    }

    HYP_FORCE_INLINE typename std::remove_reference_t<T>& Get()
    {
        AssertThrow(m_ptr != nullptr);

        return *m_ptr;
    }

    HYP_FORCE_INLINE const typename std::remove_reference_t<T>& Get() const
    {
        AssertThrow(m_ptr != nullptr);

        return *m_ptr;
    }

    HYP_FORCE_INLINE typename std::remove_reference_t<T>& GetOr(typename std::remove_reference_t<T>& defaultValue)
    {
        if (m_ptr != nullptr)
        {
            return *m_ptr;
        }

        return defaultValue;
    }

    HYP_FORCE_INLINE const typename std::remove_reference_t<T>& GetOr(const typename std::remove_reference_t<T>& defaultValue) const
    {
        if (m_ptr != nullptr)
        {
            return *m_ptr;
        }

        return defaultValue;
    }

    HYP_FORCE_INLINE NormalizedType<T> GetOr(NormalizedType<T>&& defaultValue) const&
    {
        if (m_ptr != nullptr)
        {
            return *m_ptr;
        }

        return std::move(defaultValue);
    }

    HYP_FORCE_INLINE NormalizedType<T> GetOr(NormalizedType<T>&& defaultValue) &&
    {
        if (m_ptr != nullptr)
        {
            return std::move(*m_ptr);
        }

        return std::move(defaultValue);
    }

    template <class Function, typename = std::enable_if_t<std::is_invocable_v<NormalizedType<Function>>>>
    HYP_FORCE_INLINE decltype(auto) GetOr(Function&& func) const&
    {
        if (m_ptr != nullptr)
        {
            return *m_ptr;
        }

        return func();
    }

    template <class Function, typename = std::enable_if_t<std::is_invocable_v<NormalizedType<Function>>>>
    HYP_FORCE_INLINE decltype(auto) GetOr(Function&& func) &&
    {
        if (m_ptr != nullptr)
        {
            return std::move(*m_ptr);
        }

        return func();
    }

    /*! \brief Replace the reference that is held with a new reference.
     *  \note This changes the pointer to the value we're referencing, it will not modify the current value. */
    HYP_FORCE_INLINE void Set(typename std::remove_reference_t<T>& ref)
    {
        *this = ref;
    }

    /*! \brief Remove the held value, setting the Optional<> to a default state. */
    HYP_FORCE_INLINE void Unset()
    {
        m_ptr = nullptr;
    }

    /*! \brief Construct the held value in-place. */
    HYP_FORCE_INLINE void Emplace(T& value)
    {
        m_ptr = &value;
    }

    HYP_FORCE_INLINE std::remove_reference_t<T>* operator->()
    {
        return m_ptr;
    }

    HYP_FORCE_INLINE const std::remove_reference_t<T>* operator->() const
    {
        return m_ptr;
    }

    HYP_FORCE_INLINE std::remove_reference_t<T>& operator*()
    {
        return Get();
    }

    HYP_FORCE_INLINE const std::remove_reference_t<T>& operator*() const
    {
        return Get();
    }

    HYP_FORCE_INLINE bool HasValue() const
    {
        return m_ptr != nullptr;
    }

    HYP_FORCE_INLINE bool Any() const
    {
        return m_ptr != nullptr;
    }

    HYP_FORCE_INLINE bool Empty() const
    {
        return m_ptr == nullptr;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        if (m_ptr != nullptr)
        {
            return HashCode::GetHashCode(Get());
        }

        return HashCode();
    }

private:
    typename std::remove_reference_t<T>* m_ptr;
};

} // namespace utilities

using utilities::Optional;

} // namespace hyperion

#endif