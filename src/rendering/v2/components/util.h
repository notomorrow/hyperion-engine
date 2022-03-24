#ifndef HYPERION_V2_UTIL_H
#define HYPERION_V2_UTIL_H

#include <math/math_util.h>

#include <vector>
#include <memory>
#include <algorithm>

#define HYPERION_V2_ADD_OBJECT_SPARSE 0

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

template <class T>
struct ObjectHolder {
    bool defer_create = false;
    std::vector<std::unique_ptr<T>> objects;

    inline constexpr size_t Size() const
        { return objects.size(); }

    inline constexpr
        T *Get(const typename T::ID &id)
    {
        return MathUtil::InRange(id.GetValue(), { 1, objects.size() + 1 })
            ? objects[id.GetValue() - 1].get()
            : nullptr;
    }

    inline constexpr
        const T *Get(const typename T::ID &id) const
    {
        return const_cast<ObjectHolder<T> *>(this)->Get(id);
    }

    template <class LambdaFunction>
    inline constexpr
        typename T::ID Find(LambdaFunction lambda) const
    {
        const auto it = std::find_if(objects.begin(), objects.end(), lambda);

        if (it != objects.end()) {
            return typename T::ID{typename T::ID::InnerType_t(it - objects.begin() + 1)};
        }

        return typename T::ID{0};
    }

    auto NextId() const -> typename T::ID
    {
        return typename T::ID{typename T::ID::InnerType_t(objects.size() + 1)};
    }

    template <class ...Args>
    auto Add(Engine *engine, std::unique_ptr<T> &&object, Args &&... args) -> typename T::ID
    {
        auto result = NextId();

        if (!defer_create) {
            object->Create(engine, std::move(args)...);
        }

#if HYPERION_V2_ADD_OBJECT_SPARSE
        /* Find an existing slot */
        auto it = std::find(objects.begin(), objects.end(), nullptr);

        if (it != objects.end()) {
            result = T::ID(it - objects.begin() + 1);
            objects[it] = std::move(object);

            return result;
        }
#endif

        objects.push_back(std::move(object));

        return result;
    }

    template<class ...Args>
    inline void Remove(Engine *engine, typename T::ID id, Args &&... args)
    {
        if (T *object = Get(id)) {
            object->Destroy(engine, std::move(args)...);

            /* Cannot simply erase from vector, as that would invalidate existing IDs */
            objects[id.value - 1].reset();
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