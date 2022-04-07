#ifndef HYPERION_V2_UTIL_H
#define HYPERION_V2_UTIL_H

#include <math/math_util.h>
#include <util/range.h>

#include <vector>
#include <memory>
#include <algorithm>
#include <queue>

#define HYP_ADD_OBJECT_USE_QUEUE 1

namespace hyperion::v2 {

class Engine;

template <class T>
struct ObjectIdHolder {
    std::vector<typename T::ID> ids;

    inline constexpr size_t Size() const
        { return ids.size(); }
    
    void Add(typename T::ID id)
    {
        ids.push_back(id);
    }

    void Remove(typename T::ID id)
    {
        const auto it = std::find(ids.begin(), ids.end(), id);

        if (it != ids.end()) {
            ids.erase(it);
        }
    }

    bool Has(typename T::ID id) const
    {
        return std::any_of(ids.begin(), ids.end(), [id](auto &it) { return it == id; });
    }
};

template <class CallbacksClass>
struct ComponentEvents {
    struct CallbackGroup {
        using CallbackFunction = typename CallbacksClass::CallbackFunction;

        std::vector<CallbackFunction> callbacks;

        template <class ...Args>
        void operator()(Args &&... args)
        {
            for (auto &callback : callbacks) {
                callback(args...);
            }
        }

        CallbackGroup &operator+=(const CallbackFunction &callback)
        {
            callbacks.push_back(callback);

            return *this;
        }

        CallbackGroup &operator+=(CallbackFunction &&callback)
        {
            callbacks.push_back(std::move(callback));

            return *this;
        }

        inline void Clear() { callbacks.clear(); }
    };

    CallbackGroup on_init,
                  on_deinit,
                  on_update;
};

/* Map from ObjectType::ID to another resource */
template <class ObjectType, class ValueType>
class ObjectIdMap {
public:
    ObjectIdMap() = default;
    ObjectIdMap(const ObjectIdMap &other) = delete;
    ObjectIdMap &operator=(const ObjectIdMap &other) = delete;
    ObjectIdMap(ObjectIdMap &&other) noexcept
        : m_index_map(std::move(other.m_index_map)),
          m_values(std::move(other.m_values)),
          m_max_index(other.m_max_index)
    {}
    ~ObjectIdMap() = default;

    ObjectIdMap &operator=(ObjectIdMap &&other) noexcept
    {
        m_index_map = std::move(other.m_index_map);
        m_values = std::move(other.m_values);
        m_max_index = other.m_max_index;

        return *this;
    }

    bool Has(typename ObjectType::ID id) const
    {
        const size_t index = id.Value() - 1;

        return MathUtil::InRange(index, {0, m_index_map.size()}) && m_index_map[index] != 0;
    }

    ValueType &GetOrInsert(typename ObjectType::ID id, ValueType &&value = {})
    {
        if (!Has(id)) {
            Set(id, std::move(value));
        }

        return m_values[m_index_map[id.Value() - 1] - 1];
    }

    ValueType &Get(typename ObjectType::ID id)
        { return m_values[m_index_map[id.Value() - 1] - 1]; }

    const ValueType &Get(typename ObjectType::ID id) const
        { return m_values[m_index_map[id.Value() - 1] - 1]; }

    void Set(typename ObjectType::ID id, ValueType &&value)
    {
        EnsureIndexMapIncludes(id);

        const size_t id_index = id.Value() - 1;

        size_t &index = m_index_map[id_index];

        if (index == 0) {
            m_values.push_back(std::move(value));

            index = m_values.size();
        } else {
            m_values[index - 1] = std::move(value);
        }

        m_max_index = MathUtil::Max(m_max_index, id_index);
    }

    void Remove(typename ObjectType::ID id)
    {
        if (!Has(id)) {
            return;
        }

        const size_t index = id.Value() - 1;
        const size_t index_value = m_index_map[index];

        if (index_value != 0) {
            /* For all indices proceeding the index of the removed item, we'll have to set their index to (current - 1) */
            for (size_t i = index; i <= m_max_index; ++i) {
                if (m_index_map[i] != 0) {
                    --m_index_map[i];
                }
            }

            /* Remove our value */
            m_values.erase(m_values.begin() + index_value - 1);
        }

        m_index_map[index] = 0;

        /* If our index is the last in the list, remove any indices below that may be pending removal to shrink the vector. */
        if (m_max_index == index) {
            Range<size_t> index_remove_range{index, m_index_map.size()};

            while (m_index_map[m_max_index] == 0) {
                index_remove_range |= {m_max_index, m_max_index + 1};

                if (m_max_index == 0) {
                    break;
                }
                
                --m_max_index;
            }

            m_index_map.erase(m_index_map.begin() + index_remove_range.GetStart(), m_index_map.end());
        }
    }

    void Clear()
    {
        m_max_index = 0;
        m_index_map.clear();
        m_values.clear();
    }

    inline ValueType &operator[](typename ObjectType::ID id)
        { return GetOrInsert(id); }

    ValueType *Data() const { return m_values.data(); }
    size_t Size() const { return m_values.size(); }

    auto begin() { return m_values.begin(); }
    auto end() { return m_values.end(); }

private:
    bool HasId(typename ObjectType::ID id)
    {
        const auto id_value = id.Value() - 1;

        return MathUtil::InRange(id_value, {0, m_index_map.size()});
    }

    void EnsureIndexMapIncludes(typename ObjectType::ID id)
    {
        const auto id_value = id.Value() - 1;

        if (!MathUtil::InRange(id_value, {0, m_index_map.size()})) {
            /* Resize to next power of 2 of the INDEX we will need. */
            const auto next_power_of_2 = MathUtil::NextPowerOf2(id_value + 1);

            m_index_map.resize(next_power_of_2);
        }
    }

    std::vector<size_t> m_index_map;
    std::vector<ValueType> m_values;
    size_t m_max_index = 0;
};


template <class T>
struct ObjectHolder {
    bool defer_create = false;
    std::vector<std::unique_ptr<T>> objects;

#if HYP_ADD_OBJECT_USE_QUEUE
    std::queue<size_t> free_slots;
#endif

    constexpr size_t Size() const
        { return objects.size(); }

    constexpr T *Get(const typename T::ID &id)
    {
        return MathUtil::InRange(id.Value(), {1, objects.size() + 1})
            ? objects[id.Value() - 1].get()
            : nullptr;
    }

    constexpr const T *Get(const typename T::ID &id) const
    {
        return const_cast<ObjectHolder<T> *>(this)->Get(id);
    }

    constexpr T *operator[](const typename T::ID &id) { return Get(id); }
    constexpr const T *operator[](const typename T::ID &id) const { return Get(id); }

    template <class LambdaFunction>
    constexpr typename T::ID Find(LambdaFunction lambda) const
    {
        const auto it = std::find_if(objects.begin(), objects.end(), lambda);

        if (it != objects.end()) {
            return typename T::ID{typename T::ID::ValueType(it - objects.begin() + 1)};
        }

        return T::bad_id;
    }
    
    T *Allot(std::unique_ptr<T> &&object)
    {
#if HYP_ADD_OBJECT_USE_QUEUE
        if (!free_slots.empty()) {
            const auto next_id = typename T::ID{typename T::ID::ValueType(free_slots.front() + 1)};

            object->SetId(next_id);

            objects[free_slots.front()] = std::move(object);

            free_slots.pop();

            return objects[next_id.Value() - 1].get();
        }
#endif

        const auto next_id = typename T::ID{typename T::ID::ValueType(objects.size() + 1)};

        object->SetId(next_id);

        objects.push_back(std::move(object));

        return objects[next_id.Value() - 1].get();
    }
    
    template <class ...Args>
    auto Add(Engine *engine, std::unique_ptr<T> &&object, Args &&... args) -> typename T::ID
    {
#if HYP_ADD_OBJECT_USE_QUEUE
        if (!free_slots.empty()) {
            const auto next_id = typename T::ID{typename T::ID::ValueType(free_slots.front() + 1)};

            object->SetId(next_id);

            if (!defer_create) {
                object->Create(engine, std::move(args)...);
            }

            objects[free_slots.front()] = std::move(object);

            free_slots.pop();

            return next_id;
        }
#endif

        const auto next_id = typename T::ID{typename T::ID::ValueType(objects.size() + 1)};

        object->SetId(next_id);

        if (!defer_create) {
            object->Create(engine, std::move(args)...);
        }

        objects.push_back(std::move(object));
            
        return next_id;
    }

    template<class ...Args>
    inline void Remove(Engine *engine, typename T::ID id, Args &&... args)
    {
        if (T *object = Get(id)) {
            object->Destroy(engine, std::move(args)...);

            /* Cannot simply erase from vector, as that would invalidate existing IDs */
            objects[id.value - 1].reset();

#if HYP_ADD_OBJECT_USE_QUEUE
            free_slots.push(id.value - 1);
#endif
        }
    }

    template<class ...Args>
    void RemoveAll(Engine *engine, Args &&...args)
    {
        for (auto &object : objects) {
            if (object == nullptr) {
                continue;
            }

            object->Destroy(engine, std::move(args)...);
            object.reset();
        }
    }

    template <class ...Args>
    void CreateAll(Engine *engine, Args &&... args)
    {
        AssertThrowMsg(defer_create, "Expected defer_create to be true, "
            "otherwise objects are automatically have Create() called when added.");

        for (auto &object : objects) {
            object->Create(engine, std::move(args)...);
        }
    }
};

template <class T>
class RefCountedObjectHolder {
    struct RefCount {
        uint32_t count = 0;
    };

    ObjectIdMap<T, RefCount> m_ref_counts;
    ObjectHolder<T>          m_holder;
public:
    struct RefWrapper {
        operator T *()             { return ptr; }
        operator const T *() const { return ptr; }
        operator bool() const      { return ptr != nullptr; }

        bool operator==(const RefWrapper &other) const
            { return &other == this || other.ptr == ptr; }

        bool operator!=(const RefWrapper &other) const
            { return !operator==(other); }

        T *operator->() { return ptr; }
        const T *operator->() const { return ptr; }

        T &operator*() { return *ptr; }
        const T &operator*() const { return *ptr; }

        template <class ...Args>
        void Acquire(Engine *engine, Args &&... args)
        {
            ref_holder.Acquire(engine, ptr, std::move(args)...);
        }

        template <class ...Args>
        void Release(Engine *engine, Args &&... args)
        {
            ref_holder.Release(engine, ptr, std::move(args)...);

            ptr = nullptr;
        }
        
        RefCountedObjectHolder &ref_holder;
        T *ptr = nullptr;
    };

    RefCountedObjectHolder() = default;
    ~RefCountedObjectHolder()
    {
        for (RefCount &rc : m_ref_counts) {
            if (rc.count == 0) { /* not yet initialized */
                DebugLog(
                    LogType::Warn,
                    "Ref to object of type %s was never initialized\n",
                    typeid(T).name()
                );
            } else {
                --rc.count;
            }

            AssertThrowMsg(rc.count == 0, "Destructor called while object still in use elsewhere");
        }
    }

    template <class ...Args>
    T *Acquire(Engine *engine, T *ptr, Args &&... args)
    {
        AssertThrow(ptr != nullptr);

        auto &ref_count = m_ref_counts[ptr->GetId()].count;

        if (ref_count == 0) {// && !ptr->IsInitialized()) {
            ptr->Create(engine, std::move(args)...);
        }

        ++ref_count;

        return ptr;
    }

    template <class ...Args>
    T *Acquire(Engine *engine, const typename T::ID &id, Args &&... args)
        { return Acquire(engine, m_holder.Get(id), std::move(args)...); }

    template <class ...Args>
    void Release(Engine *engine, const T *ptr, Args &&... args)
    {
        AssertThrow(ptr != nullptr);

        const auto id = ptr->GetId();

        AssertThrowMsg(m_ref_counts[id].count != 0, "Cannot decrement refcount when already at zero (or not set)");
        
        if (!--m_ref_counts[id].count) {
            m_holder.Remove(engine, id, std::forward<Args>(args)...);
            m_ref_counts.Remove(id);
        }
    }

    void Release(Engine *engine, const typename T::ID &id)
        { return Release(engine, m_holder.Get(id)); }

    template <class ...Args>
    RefWrapper Add(Args &&... args)
    {
        return RefWrapper{
            .ref_holder = *this,
            .ptr        = m_holder.Allot(std::make_unique<T>(std::forward<Args>(args)...))
        };
    }

    T *Get(const typename T::ID &id) const
        { return m_holder.Get(id); }

    size_t GetRefCount(const typename T::ID &id) const
    {
        if (!m_ref_counts.Has(id)) {
            return 0;
        }

        return m_ref_counts.Get(id).count;
    }
};

template <class T>
using RefWrapper = typename RefCountedObjectHolder<T>::RefWrapper;

} // namespace hyperion::v2

#endif