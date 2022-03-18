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
struct ObjectHolder {
    bool defer_create = false;
    std::vector<std::unique_ptr<T>> objects;

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

    template <class ...Args>
    auto Add(Engine *engine, std::unique_ptr<T> &&object, Args &&... args) -> typename T::ID
    {
        typename T::ID result{};

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

        return typename T::ID{ typename T::ID::InnerType_t(objects.size()) };
    }

    template<class ...Args>
    inline void Remove(Engine *engine, typename T::ID id, Args &&... args)
    {
        auto &object = objects[id];

        AssertThrowMsg(object != nullptr, "Failed to remove object with id %d -- object was nullptr.", id.GetValue());

        object->Destroy(engine, std::move(args)...);

        /* Cannot simply erase from vector, as that would invalidate existing IDs */
        objects[id].reset();
    }

    template<class ...Args>
    void RemoveAll(Engine *engine, Args &&...args)
    {
        for (auto &object : objects) {
            object->Destroy(engine, args...);
            object.reset();
        }
    }

    template <class ...Args>
    void CreateAll(Engine *engine, Args &&... args)
    {
        AssertThrowMsg(defer_create, "Expected defer_create to be true, "
            "otherwise objects are automatically have Create() called when added.");

        for (auto &object : objects) {
            object->Create(engine, args...);
        }
    }
};

} // namespace hyperion::v2

#endif