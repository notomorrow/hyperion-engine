#ifndef HYPERION_V2_CORE_HANDLE_HPP
#define HYPERION_V2_CORE_HANDLE_HPP

#include <core/Core.hpp>
#include <core/ID.hpp>
#include <core/ObjectPool.hpp>

namespace hyperion::v2 {

template <class T>
class ObjectContainer;

template <class T>
static inline ObjectContainer<T> &GetContainer()
{
    return GetObjectPool().template GetContainer<T>();
}

template <class T>
struct Handle
{
    using IDType = ID<T>;

    static_assert(has_opaque_handle_defined<T>, "Type does not support handles");

    static const Handle empty;

    UInt index;

    Handle()
        : index(0)
    {
    }

    explicit Handle(IDType id)
        : index(id.Value())
    {
        if (index != 0) {
            GetContainer<T>().IncRefStrong(index - 1);
        }
    }

    Handle(const Handle &other)
        : index(other.index)
    {
        if (index != 0) {
            GetContainer<T>().IncRefStrong(index - 1);
        }
    }

    Handle &operator=(const Handle &other)
    {
        if (other.index == index) {
            return *this;
        }

        if (index != 0) {
            GetContainer<T>().DecRefStrong(index - 1);
        }

        index = other.index;

        if (index != 0) {
            GetContainer<T>().IncRefStrong(index - 1);
        }

        return *this;
    }

    Handle(Handle &&other) noexcept
        : index(other.index)
    {
        other.index = 0;
    }

    Handle &operator=(Handle &&other) noexcept
    {
        if (other.index == index) {
            return *this;
        }

        if (index != 0) {
            GetContainer<T>().DecRefStrong(index - 1);
        }

        index = other.index;
        other.index = 0;

        return *this;
    }

    ~Handle()
    {
        if (index != 0) {
            GetContainer<T>().DecRefStrong(index - 1);
        }
    }

    T *operator->() const
        { return Get(); }

    T &operator*()
        { return *Get(); }

    const T &operator*() const
        { return *Get(); }

    bool operator!() const
        { return !IsValid(); }

    explicit operator bool() const
        { return IsValid(); }

    bool operator==(std::nullptr_t) const
        { return !IsValid(); }

    bool operator!=(std::nullptr_t) const
        { return IsValid(); }

    bool operator==(const Handle &other) const
        { return index == other.index; }

    bool operator!=(const Handle &other) const
        { return index == other.index; }

    bool operator<(const Handle &other) const
        { return index < other.index; }

    bool IsValid() const
        { return index != 0; }

    IDType GetID() const
        { return { UInt(index) }; }

    T *Get() const
    {
        if (index == 0) {
            return nullptr;
        }

        return &GetContainer<T>().Get(index - 1);
    }

    void Reset()
    {
        auto &container = GetContainer<T>();  

        if (index != 0) {
            container.DecRefStrong(index - 1);
        }

        index = 0;
    }

    HYP_FORCE_INLINE static Name GetTypeName()
        { return HandleDefinition<T>::GetNameForType(); }

    HYP_FORCE_INLINE static constexpr const char *GetClassNameString()
        { return HandleDefinition<T>::GetClassNameString(); }

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

    UInt index;

    WeakHandle()
        : index(0)
    {
    }

    WeakHandle(const Handle<T> &other)
        : index(other.index)
    {
        if (index != 0) {
            GetContainer<T>().IncRefWeak(index - 1);
        }
    }

    WeakHandle &operator=(const Handle<T> &other)
    {
        if (index != 0) {
            GetContainer<T>().DecRefWeak(index - 1);
        }

        index = other.index;

        if (index != 0) {
            GetContainer<T>().IncRefWeak(index - 1);
        }

        return *this;
    }

    WeakHandle(const WeakHandle &other)
        : index(other.index)
    {
        if (index != 0) {
            GetContainer<T>().IncRefWeak(index - 1);
        }
    }

    WeakHandle &operator=(const WeakHandle &other)
    {
        if (index != 0) {
            GetContainer<T>().DecRefWeak(index - 1);
        }

        index = other.index;

        if (index != 0) {
            GetContainer<T>().IncRefWeak(index - 1);
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
            GetContainer<T>().DecRefWeak(index - 1);
        }

        index = other.index;
        other.index = 0;

        return *this;
    }

    ~WeakHandle()
    {
        if (index != 0) {
            GetContainer<T>().DecRefWeak(index - 1);
        }
    }

    Handle<T> Lock() const
        { return Handle<T>(IDType { index }); }

    bool operator!() const
        { return !IsValid(); }

    explicit operator bool() const
        { return IsValid(); }

    bool operator==(std::nullptr_t) const
        { return !IsValid(); }

    bool operator!=(std::nullptr_t) const
        { return IsValid(); }

    bool operator==(const WeakHandle &other) const
        { return index == other.index; }

    bool operator!=(const WeakHandle &other) const
        { return index == other.index; }

    bool operator<(const WeakHandle &other) const
        { return index < other.index; }

    bool operator==(const Handle<T> &other) const
        { return index == other.index; }

    bool operator!=(const Handle<T> &other) const
        { return index == other.index; }

    bool operator<(const Handle<T> &other) const
        { return index < other.index; }

    bool IsValid() const
        { return index != 0; }

    IDType GetID() const
        { return { UInt(index) }; }

    void Reset()
    {
        auto &container = GetContainer<T>();  

        if (index != 0) {
            container.DecRefWeak(index - 1);
        }

        index = 0;
    }

    HYP_FORCE_INLINE static Name GetTypeName()
        { return HandleDefinition<T>::GetNameForType(); }

    HYP_FORCE_INLINE static constexpr const char *GetClassNameString()
        { return HandleDefinition<T>::GetClassNameString(); }

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

template <class T, class Engine, class ...Args>
static HYP_FORCE_INLINE Handle<T> CreateObjectIntern(Engine *engine, Args &&... args)
{
    return engine->template CreateObject<T>(std::forward<Args>(args)...);
}

/**
 * \brief Creates a new object of type T. The object is not initialized until InitObject() is called with the object as a parameter.
 */
template <class T, class ...Args>
static HYP_FORCE_INLINE Handle<T> CreateObject(Args &&... args)
{
    return CreateObjectIntern<T>(GetEngine(), std::forward<Args>(args)...);
}

// Version of CreateObject for within scripts,
// taking single void * parameter for the allocated object
template <class T>
static HYP_FORCE_INLINE Handle<T> ScriptCreateObject(const void *)
{
    return CreateObject<T>();
}

template <class T>
static HYP_FORCE_INLINE ID<T> ScriptGetHandleID(const Handle<T> &handle)
{
    return handle.GetID();
}

template <class T>
static HYP_FORCE_INLINE typename ID<T>::ValueType ScriptGetHandleIDValue(const Handle<T> &handle)
{
    return handle.GetID().Value();
}

} // namespace hyperion::v2

#endif