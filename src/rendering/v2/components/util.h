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

        return MathUtil::InRange(index, {0, m_max_index + 1}) && m_index_map[index] != 0;
    }

    ValueType &GetOrInsert(typename ObjectType::ID id, ValueType &&value = ValueType())
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

    ValueType *Data() const { return m_values.data(); }
    size_t Size() const { return m_values.size(); }

    auto begin() const { return m_values.begin(); }
    auto end() const { return m_values.end(); }

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

    inline constexpr size_t Size() const
        { return objects.size(); }

    inline constexpr
        T *Get(const typename T::ID &id)
    {
        return MathUtil::InRange(id.Value(), { 1, objects.size() + 1 })
            ? objects[id.Value() - 1].get()
            : nullptr;
    }

    inline constexpr
        const T *Get(const typename T::ID &id) const
    {
        return const_cast<ObjectHolder<T> *>(this)->Get(id);
    }

    inline constexpr T *operator[](const typename T::ID &id) { return Get(id); }
    inline constexpr const T *operator[](const typename T::ID &id) const { return Get(id); }

    template <class LambdaFunction>
    inline constexpr
        typename T::ID Find(LambdaFunction lambda) const
    {
        const auto it = std::find_if(objects.begin(), objects.end(), lambda);

        if (it != objects.end()) {
            return typename T::ID{typename T::ID::ValueType(it - objects.begin() + 1)};
        }

        return typename T::ID{0};
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

} // namespace hyperion::v2

#endif