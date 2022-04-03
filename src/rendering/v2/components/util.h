#ifndef HYPERION_V2_UTIL_H
#define HYPERION_V2_UTIL_H

#include <math/math_util.h>

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

struct EngineCallbacks {
    using CallbackFunction = std::function<void(Engine *)>;
};

template <class CallbacksClass>
struct ComponentEvents {
    struct Callbacks {
        using CallbackFunction = typename CallbacksClass::CallbackFunction;

        std::vector<CallbackFunction> callbacks;

        template <class ...Args>
        void operator()(Args &&... args)
        {
            for (auto &callback : callbacks) {
                callback(args...);
            }
        }

        Callbacks &operator+=(const CallbackFunction &callback)
        {
            callbacks.push_back(callback);

            return *this;
        }

        Callbacks &operator+=(CallbackFunction &&callback)
        {
            callbacks.push_back(std::move(callback));

            return *this;
        }

        inline void Clear() { callbacks.clear(); }
    };

    Callbacks on_init,
              on_deinit,
              on_update;
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