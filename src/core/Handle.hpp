/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HANDLE_HPP
#define HYPERION_CORE_HANDLE_HPP

#include <core/Core.hpp>
#include <core/ID.hpp>
#include <core/ObjectPool.hpp>

namespace hyperion {

template <class T>
class ObjectContainer;

template <class T>
static inline ObjectContainer<T> &GetContainer(UniquePtr<ObjectContainerBase> *allotted_container)
{
    return ObjectPool::template GetContainer<T>(allotted_container);
}

template <class T>
static inline UniquePtr<ObjectContainerBase> *AllotContainer()
{
    return ObjectPool::template AllotContainer<T>();
}

struct HandleBase
{
    uint index;

protected:
    HandleBase() : index(0) { }
    HandleBase(uint index) : index(index) { }
    HandleBase(const HandleBase &other)                 = default;
    HandleBase &operator=(const HandleBase &other)      = default;
    HandleBase(HandleBase &other) noexcept              = default;
    HandleBase &operator=(HandleBase &&other) noexcept  = default;
};

/*! \brief Definition of a handle for a type.
 *  \details In Hyperion, a handle is a reference to an instance of a specific core engine object type.
 *  These objects are managed by the ObjectPool and are reference counted.
 *  \note Objects that are managed by the ObjectPool must have a HandleDefinition defined for them.
 *  Check the HandleDefinitions.inl file for list of defined handles.
 *  \tparam T The type of object that the handle is referencing. 
*/
template <class T>
struct Handle : HandleBase
{
    using IDType = ID<T>;

    static_assert(has_opaque_handle_defined<T>, "Type does not support handles");
    
    static ObjectContainer<T> *s_container;

    static ObjectContainer<T> &GetContainer()
    {
        static struct Initializer
        {
            Initializer()
            {
                s_container = &(ObjectPool::template GetContainer<T>(HandleDefinition<T>::GetAllottedContainerPointer()));
            }
        } initializer;

        return *s_container;
    }

    static const Handle empty;

    Handle() : HandleBase() { }

    /*! \brief Construct a handle from the given ID.
     *  \param id The ID of the object to reference. */
    explicit Handle(IDType id)
        : HandleBase(id.Value())
    {
        if (index != 0) {
            GetContainer().IncRefStrong(index - 1);
        }
    }

    Handle(const Handle &other)
        : HandleBase(static_cast<const HandleBase &>(other))
    {
        if (index != 0) {
            GetContainer().IncRefStrong(index - 1);
        }
    }

    Handle &operator=(const Handle &other)
    {
        if (other.index == index) {
            return *this;
        }

        if (index != 0) {
            GetContainer().DecRefStrong(index - 1);
        }

        index = other.index;

        if (index != 0) {
            GetContainer().IncRefStrong(index - 1);
        }

        return *this;
    }

    Handle(Handle &&other) noexcept
        : HandleBase(static_cast<HandleBase &&>(other))
    {
        other.index = 0;
    }

    Handle &operator=(Handle &&other) noexcept
    {
        if (other.index == index) {
            return *this;
        }

        if (index != 0) {
            GetContainer().DecRefStrong(index - 1);
        }

        index = other.index;
        other.index = 0;

        return *this;
    }

    ~Handle()
    {
        if (index != 0) {
            GetContainer().DecRefStrong(index - 1);
        }
    }
    
    HYP_FORCE_INLINE
    T *operator->() const
        { return Get(); }
    
    HYP_FORCE_INLINE
    T &operator*()
        { return *Get(); }
    
    HYP_FORCE_INLINE
    const T &operator*() const
        { return *Get(); }
    
    HYP_FORCE_INLINE
    bool operator!() const
        { return !IsValid(); }
    
    HYP_FORCE_INLINE
    explicit operator bool() const
        { return IsValid(); }
    
    HYP_FORCE_INLINE
    bool operator==(std::nullptr_t) const
        { return !IsValid(); }
    
    HYP_FORCE_INLINE
    bool operator!=(std::nullptr_t) const
        { return IsValid(); }
    
    HYP_FORCE_INLINE
    bool operator==(const Handle &other) const
        { return index == other.index; }
    
    HYP_FORCE_INLINE
    bool operator!=(const Handle &other) const
        { return index != other.index; }
    
    /*! \brief Compare two handles by their index.
     *  \param other The handle to compare to.
     *  \return True if the handle is less than the other handle. */
    HYP_FORCE_INLINE
    bool operator<(const Handle &other) const
        { return index < other.index; }
    
    HYP_FORCE_INLINE
    bool operator==(const IDType &id) const
        { return index == id.Value(); }
    
    HYP_FORCE_INLINE
    bool operator!=(const IDType &id) const
        { return index != id.Value(); }

    HYP_FORCE_INLINE
    bool operator<(const IDType &id) const
        { return index < id.Value(); }
    
    /*! \brief Check if the handle is valid. A handle is valid if its index is greater than 0.
     *  \return True if the handle is valid. */
    HYP_FORCE_INLINE
    bool IsValid() const
        { return index != 0; }
    
    /*! \brief Get a referenceable ID for the object that the handle is referencing.
     *  \return The ID of the object. */
    HYP_FORCE_INLINE
    IDType GetID() const
        { return { uint(index) }; }
    
    /*! \brief Get a pointer to the object that the handle is referencing.
     *  \return A pointer to the object. */
    HYP_FORCE_INLINE
    T *Get() const
    {
        if (index == 0) {
            return nullptr;
        }

        return &GetContainer().Get(index - 1);
    }
    
    /*! \brief Reset the handle to an empty state.
     *  \details This will decrement the strong reference count of the object that the handle is referencing.
     *  The index is set to 0. */
    HYP_FORCE_INLINE
    void Reset()
    {
        if (index != 0) {
            GetContainer().DecRefStrong(index - 1);
        }

        index = 0;
    }
    
    HYP_FORCE_INLINE
    static Name GetTypeName()
        { return HandleDefinition<T>::GetNameForType(); }
    
    HYP_FORCE_INLINE
    static constexpr const char *GetClassNameString()
        { return HandleDefinition<T>::GetClassNameString(); }
    
    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(GetTypeName().GetHashCode());
        hc.Add(index);

        return hc;
    }
};

template <class T>
struct WeakHandle
{
    using IDType = ID<T>;

    static_assert(has_opaque_handle_defined<T>, "Type does not support handles");
    
    static ObjectContainer<T> *s_container;

    static ObjectContainer<T> &GetContainer()
    {
        static struct Initializer
        {
            ObjectContainer<T> *ptr;

            Initializer()
                : ptr(nullptr)
            {
                WeakHandle<T>::s_container = &(ObjectPool::template GetContainer<T>(HandleDefinition<T>::GetAllottedContainerPointer()));
                ptr = s_container;
            }
        } initializer;

        return *s_container;
    }

    /*! \brief The index of the object in the object pool's container for \ref{T} */
    uint index;

    WeakHandle()
        : index(0)
    {
    }

    WeakHandle(const Handle<T> &other)
        : index(other.index)
    {
        if (index != 0) {
            GetContainer().IncRefWeak(index - 1);
        }
    }

    WeakHandle &operator=(const Handle<T> &other)
    {
        if (index != 0) {
            GetContainer().DecRefWeak(index - 1);
        }

        index = other.index;

        if (index != 0) {
            GetContainer().IncRefWeak(index - 1);
        }

        return *this;
    }

    WeakHandle(const WeakHandle &other)
        : index(other.index)
    {
        if (index != 0) {
            GetContainer().IncRefWeak(index - 1);
        }
    }

    WeakHandle &operator=(const WeakHandle &other)
    {
        if (index != 0) {
            GetContainer().DecRefWeak(index - 1);
        }

        index = other.index;

        if (index != 0) {
            GetContainer().IncRefWeak(index - 1);
        }

        return *this;
    }

    WeakHandle(WeakHandle &&other) noexcept
        : index(other.index)
    {
        other.index = 0;
    }

    WeakHandle &operator=(WeakHandle &&other) noexcept
    {
        if (index != 0) {
            GetContainer().DecRefWeak(index - 1);
        }

        index = other.index;
        other.index = 0;

        return *this;
    }

    ~WeakHandle()
    {
        if (index != 0) {
            GetContainer().DecRefWeak(index - 1);
        }
    }

    /*! \brief Lock the weak handle to get a strong reference to the object.
     *  \details If the object is still alive, a strong reference is returned. Otherwise, an empty handle is returned.
     *  \return A strong reference to the object. */
    HYP_NODISCARD HYP_FORCE_INLINE
    Handle<T> Lock() const
    {
        if (index == 0) {
            return Handle<T>();
        }

        return GetContainer().GetObjectBytes(index - 1).GetRefCountStrong() != 0
            ? Handle<T>(IDType { index })
            : Handle<T>();
    }
    
    HYP_FORCE_INLINE
    bool operator!() const
        { return !IsValid(); }
    
    HYP_FORCE_INLINE
    explicit operator bool() const
        { return IsValid(); }
    
    HYP_FORCE_INLINE
    bool operator==(std::nullptr_t) const
        { return !IsValid(); }
    
    HYP_FORCE_INLINE
    bool operator!=(std::nullptr_t) const
        { return IsValid(); }
    
    HYP_FORCE_INLINE
    bool operator==(const WeakHandle &other) const
        { return index == other.index; }
    
    HYP_FORCE_INLINE
    bool operator!=(const WeakHandle &other) const
        { return index != other.index; }

    HYP_FORCE_INLINE
    bool operator<(const WeakHandle &other) const
        { return index < other.index; }
    
    HYP_FORCE_INLINE
    bool operator==(const Handle<T> &other) const
        { return index == other.index; }
    
    HYP_FORCE_INLINE
    bool operator!=(const Handle<T> &other) const
        { return index != other.index; }
    
    HYP_FORCE_INLINE
    bool operator<(const Handle<T> &other) const
        { return index < other.index; }
    
    HYP_FORCE_INLINE
    bool operator==(const IDType &id) const
        { return index == id.Value(); }
    
    HYP_FORCE_INLINE
    bool operator!=(const IDType &id) const
        { return index != id.Value(); }
    
    HYP_FORCE_INLINE
    bool operator<(const IDType &id) const
        { return index < id.Value(); }
    
    HYP_FORCE_INLINE
    bool IsValid() const
        { return index != 0; }
    
    HYP_FORCE_INLINE
    IDType GetID() const
        { return { uint(index) }; }

    void Reset()
    {
        if (index != 0) {
            GetContainer().DecRefWeak(index - 1);
        }

        index = 0;
    }
    
    HYP_FORCE_INLINE static Name GetTypeName()
        { return HandleDefinition<T>::GetNameForType(); }
    
    HYP_FORCE_INLINE static constexpr const char *GetClassNameString()
        { return HandleDefinition<T>::GetClassNameString(); }
    
    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(String(HandleDefinition<T>::class_name).GetHashCode());
        hc.Add(index);

        return hc;
    }
};

template <class T>
const Handle<T> Handle<T>::empty = { };

template <class T>
ObjectContainer<T> *Handle<T>::s_container = nullptr;

template <class T>
ObjectContainer<T> *WeakHandle<T>::s_container = nullptr;

#define DEF_HANDLE(T, _max_size) \
    class T; \
    \
    extern UniquePtr<ObjectContainerBase> *g_container_ptr_##T; \
    \
    template <> \
    struct HandleDefinition< T > \
    { \
        static constexpr const char *class_name = HYP_STR(T); \
        static constexpr SizeType max_size = (_max_size); \
        \
        static constexpr const char *GetClassNameString() \
        { \
            return class_name; \
        } \
        \
        static Name GetNameForType() \
        { \
            static const Name name = NAME(HYP_STR(T)); \
            return name; \
        } \
        \
        HYP_API static UniquePtr<ObjectContainerBase> *GetAllottedContainerPointer(); \
    };

#define DEF_HANDLE_NS(ns, T, _max_size) \
    namespace ns { \
    class T; \
    } \
    \
    extern UniquePtr<ObjectContainerBase> *g_container_ptr_##T; \
    \
    template <> \
    struct HandleDefinition< ns::T > \
    { \
        static constexpr const char *class_name = HYP_STR(ns) "::" HYP_STR(T); \
        static constexpr SizeType max_size = (_max_size); \
        \
        static constexpr const char *GetClassNameString() \
        { \
            return class_name; \
        } \
        \
        static Name GetNameForType() \
        { \
            static const Name name = NAME(HYP_STR(ns::T)); \
            return name; \
        } \
        \
        HYP_API static UniquePtr<ObjectContainerBase> *GetAllottedContainerPointer(); \
    };

#include <core/inl/HandleDefinitions.inl>

#undef DEF_HANDLE
#undef DEF_HANDLE_NS

} // namespace hyperion

#endif