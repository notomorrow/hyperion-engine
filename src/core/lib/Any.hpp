#ifndef HYPERION_V2_LIB_ANY_HPP
#define HYPERION_V2_LIB_ANY_HPP

#include <core/lib/TypeID.hpp>
#include <system/Debug.hpp>
#include <Types.hpp>

#include <type_traits>

namespace hyperion {

class Any {
    using DeleteFunction = std::add_pointer_t<void(void *)>;

public:
    Any()
        : m_type_id(TypeID::ForType<void>()),
          m_ptr(nullptr),
          m_delete_function(nullptr)
    {
    }

    template <class T, class ...Args>
    static auto MakeAny(Args &&... args)
    {
        Any any;
        any.m_type_id = TypeID::ForType<T>();
        any.m_ptr = new T(std::forward<Args>(args)...);
        any.m_delete_function = [](void *ptr) { delete static_cast<T *>(ptr); };

        return std::move(any);
    }

#if 0
    template <class T>
    Any &operator=(const T &value)
    {
        const auto new_type_id = TypeID::ForType<T>();

        if (m_type_id == new_type_id) {
            // types are same, call copy constructor.
            *static_cast<T *>(m_ptr) = value;

            return *this;
        }

        if (HasValue()) {
            m_delete_function(m_ptr);
        }

        m_type_id = new_type_id;
        m_ptr = new T(value);
        m_delete_function = [](void *ptr) { delete static_cast<T *>(ptr); };

        return *this;
    }

    template <class T>
    Any(T &&value) noexcept
        : m_type_id(TypeID::ForType<T>()),
          m_ptr(new T(std::forward<T>(value))),
          m_delete_function([](void *ptr) { delete static_cast<T *>(ptr); })
    {
    }

    template <class T>
    Any &operator=(T &&value) noexcept
    {
        const auto new_type_id = TypeID::ForType<T>();

        if (m_type_id == new_type_id) {
            // types are same, call copy constructor.
            *static_cast<T *>(m_ptr) = std::move(value);

            return *this;
        }

        if (HasValue()) {
            m_delete_function(m_ptr);
        }

        m_type_id = new_type_id;
        m_ptr = new T(std::move(value));
        m_delete_function = [](void *ptr) { delete static_cast<T *>(ptr); };

        return *this;
    }
#endif

    Any(const Any &other) = delete;
    Any &operator=(const Any &other) = delete;

    Any(Any &&other) noexcept
        : m_type_id(other.m_type_id),
          m_ptr(other.m_ptr),
          m_delete_function(other.m_delete_function)
    {
        other.m_type_id = TypeID::ForType<void>();
        other.m_ptr = nullptr;
        other.m_delete_function = nullptr;
    }

    Any &operator=(Any &&other) noexcept
    {
        if (HasValue()) {
            m_delete_function(m_ptr);
        }

        m_type_id = other.m_type_id;
        m_ptr = other.m_ptr;
        m_delete_function = other.m_delete_function;

        other.m_type_id = TypeID::ForType<void>();
        other.m_ptr = nullptr;
        other.m_delete_function = nullptr;
        
        return *this;
    }

    ~Any()
    {
        if (HasValue()) {
            m_delete_function(m_ptr);
        }
    }

    bool HasValue() const
        { return m_ptr != nullptr; }

    const TypeID &GetTypeID() const
        { return m_type_id; }

    template <class T>
    bool Is() const
        { return TypeID::ForType<T>() == m_type_id; }

    template <class T>
    T &Get()
    {
        AssertThrowMsg(Is<T>(), "Held type not equal to requested type!");

        return *static_cast<T *>(m_ptr);
    }

    template <class T>
    const T &Get() const
    {
        AssertThrowMsg(Is<T>(), "Held type not equal to requested type!");

        return *static_cast<const T *>(m_ptr);
    }

    template <class T, class ...Args>
    void Set(Args &&... args)
    {
        if (HasValue()) {
            m_delete_function(m_ptr);
        }

        m_ptr = new T(std::forward<Args>(args)...);
        m_delete_function = [](void *ptr) { delete static_cast<T *>(ptr); };
    }

    /*! \brief Drop ownership of the object, giving it to the caller.
        Make sure to use delete! */
    template <class T>
    T *Release()
    {
        AssertThrowMsg(Is<T>(), "Held type not equal to requested type!");

        T *ptr = static_cast<T *>(m_ptr);

        m_type_id = TypeID::ForType<void>();;
        m_ptr = nullptr;
        m_delete_function = nullptr;

        return ptr;
    }

private:
    TypeID m_type_id;
    void *m_ptr;
    DeleteFunction m_delete_function;
};

#if 0

class Any {
    using DeleteFunction = std::add_pointer_t<void(void *)>;
    using CopyFunction = std::add_pointer_t<void *(void *)>;

public:
    Any()
        : m_type_id(TypeID::ForType<void>()),
          m_ptr(nullptr),
          m_copy_function(nullptr),
          m_delete_function(nullptr)
    {
    }

    template <class T>
    Any(const T &value)
        : m_type_id(TypeID::ForType<T>()),
          m_ptr(new T(value)),
          m_copy_function(nullptr),
          m_delete_function([](void *ptr) { delete static_cast<T *>(ptr); })
    {
        if constexpr (std::is_copy_constructible<T>) {
            m_copy_function = [](void *ptr) { return new T(static_cast<T *>(ptr)); };
        }
    }

    template <class T>
    Any &operator=(const T &value)
    {
        const auto new_type_id = TypeID::ForType<T>();

        if (m_type_id == new_type_id) {
            // types are same, call copy constructor.
            *static_cast<T *>(m_ptr) = value;

            return *this;
        }

        if (HasValue()) {
            m_delete_function(m_ptr);
        }

        m_type_id = new_type_id;
        m_ptr = new T(value);
        m_copy_function = nullptr;
        m_delete_function = [](void *ptr) { delete static_cast<T *>(ptr); };

        if constexpr (std::is_copy_constructible<T>) {
            m_copy_function = [](void *ptr) { return new T(static_cast<T *>(ptr)); };
        }

        return *this;
    }

    template <class T>
    Any(T &&value) noexcept
        : m_type_id(TypeID::ForType<T>()),
          m_ptr(new T(std::forward<T>(value))),
          m_copy_function(nullptr),
          m_delete_function([](void *ptr) { delete static_cast<T *>(ptr); })
    {
        if constexpr (std::is_copy_constructible<T>) {
            m_copy_function = [](void *ptr) { return new T(static_cast<T *>(ptr)); };
        }
    }

    template <class T>
    Any &operator=(T &&value) noexcept
    {
        const auto new_type_id = TypeID::ForType<T>();

        if (m_type_id == new_type_id) {
            // types are same, call copy constructor.
            *static_cast<T *>(m_ptr) = std::move(value);

            return *this;
        }

        if (HasValue()) {
            m_delete_function(m_ptr);
        }

        m_type_id = new_type_id;
        m_ptr = new T(std::move(value));
        m_copy_function = nullptr;
        m_delete_function = [](void *ptr) { delete static_cast<T *>(ptr); };

        if constexpr (std::is_copy_constructible<T>) {
            m_copy_function = [](void *ptr) { return new T(static_cast<T *>(ptr)); };
        }

        return *this;
    }

    Any(const Any &other)
        : m_type_id(other.m_type_id),
          m_ptr(other.HasValue() ? other.m_copy_function(other.m_ptr) : nullptr),
          m_copy_function(other.m_copy_function),
          m_delete_function(other.m_delete_function)
    {
    }

    Any &operator=(const Any &other)
    {
        if (HasValue()) {
            m_delete_function(m_ptr);
        }

        m_type_id = other.m_type_id;
        m_ptr = other.HasValue() ? other.m_copy_function(other.m_ptr) : nullptr;
        m_copy_function = other.m_copy_function;
        m_delete_function = other.m_delete_function;
        
        return *this;
    }

    Any(Any &&other) noexcept
        : m_type_id(other.m_type_id),
          m_ptr(other.m_ptr),
          m_copy_function(other.m_copy_function),
          m_delete_function(other.m_delete_function)
    {
        other.m_type_id = TypeID::ForType<void>();
        other.m_ptr = nullptr;
        other.m_copy_function = nullptr;
        other.m_delete_function = nullptr;
    }

    Any &operator=(Any &&other) noexcept
    {
        if (HasValue()) {
            m_delete_function(m_ptr);
        }

        m_type_id = other.m_type_id;
        m_ptr = other.m_ptr;
        m_copy_function = other.m_copy_function;
        m_delete_function = other.m_delete_function;

        other.m_type_id = TypeID::ForType<void>();
        other.m_ptr = nullptr;
        other.m_copy_function = nullptr;
        other.m_delete_function = nullptr;
        
        return *this;
    }

    ~Any()
    {
        if (HasValue()) {
            m_delete_function(m_ptr);
        }
    }

    bool HasValue() const
        { return m_ptr != nullptr; }

    const TypeID &GetTypeID() const
        { return m_type_id; }

    template <class T>
    bool Is() const
        { return TypeID::ForType<T>() == m_type_id; }

    template <class T>
    T &Get()
    {
        AssertThrowMsg(Is<T>(), "Held type not equal to requested type!");

        return *static_cast<T *>(m_ptr);
    }

    template <class T>
    const T &Get() const
    {
        AssertThrowMsg(Is<T>(), "Held type not equal to requested type!");

        return *static_cast<const T *>(m_ptr);
    }

    template <class T>
    void Set(const T &value)
        { *this = value; }

    template <class T>
    void Set(T &&value)
        { *this = std::forward<T>(value); }

private:
    TypeID m_type_id;
    void *m_ptr;
    DeleteFunction m_delete_function;
    CopyFunction m_copy_function;
};

#endif

} // namespace hyperion

#endif