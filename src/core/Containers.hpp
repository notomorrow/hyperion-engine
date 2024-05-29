/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_CORE_CONTAINERS_HPP
#define HYPERION_CORE_CONTAINERS_HPP

#include <core/containers/HashMap.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/containers/TypeMap.hpp>
#include <core/containers/StaticMap.hpp>
#include <core/containers/ArrayMap.hpp>
#include <core/containers/FlatSet.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/FixedArray.hpp>
#include <core/containers/HeapArray.hpp>
#include <core/containers/String.hpp>
#include <core/containers/Queue.hpp>
#include <core/containers/Stack.hpp>
#include <core/containers/LinkedList.hpp>

#include <core/memory/ByteBuffer.hpp>
#include <core/utilities/StringView.hpp>
#include <core/functional/Proc.hpp>
#include <core/memory/RefCountedPtr.hpp>
#include <core/memory/UniquePtr.hpp>
#include <core/utilities/Variant.hpp>
#include <core/utilities/Optional.hpp>
#include <core/containers/Bitset.hpp>
#include <core/utilities/Span.hpp>
#include <core/threading/AtomicVar.hpp>
#include <math/MathUtil.hpp>

#include <Types.hpp>

#include <vector>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include <tuple>
#include <mutex>

namespace hyperion {

class Engine;

template <class Group>
struct CallbackRef
{
    uint id;
    Group *group;
    typename Group::ArgsTuple bound_args;

    CallbackRef()
        : id(0),
          group(nullptr),
          bound_args{}
    {
    }

    CallbackRef(uint id, Group *group)
        : id(id),
          group(group),
          bound_args{}
    {
    }

    CallbackRef(const CallbackRef &other) = delete;
    CallbackRef &operator=(const CallbackRef &other) = delete;

    CallbackRef(CallbackRef &&other) noexcept
        : id(other.id),
          group(other.group),
          bound_args(std::move(other.bound_args))
    {
        other.id = 0;
        other.group = nullptr;
    }
    
    CallbackRef &operator=(CallbackRef &&other) noexcept
    {
        if (&other == this) {
            return *this;
        }

        if (Valid()) {
            Remove();
        }

        id = other.id;
        group = other.group;
        bound_args = std::move(other.bound_args);

        other.id = 0;
        other.group = nullptr;

        return *this;
    }

    ~CallbackRef() = default;

    bool Valid() const { return id != 0 && group != nullptr; }

    bool Remove()
    {
        const bool result = Valid() && group->Remove(id);

        id = 0;
        group = nullptr;
        bound_args = {};

        return result;
    }

    CallbackRef &Bind(typename Group::ArgsTuple &&args)
    {
        bound_args = std::move(args);

        return *this;
    }

    bool Trigger()
    {
        if (!Valid()) {
            return false;
        }

        /* expand bound_args tuple into Trigger() function args */
        return std::apply([this]<class ...Args> (Args ... args) -> bool {
            return group->Trigger(id, std::forward<Args>(args)...);
        }, bound_args);
    }

    bool TriggerRemove()
    {
        if (!Valid()) {
            return false;
        }

        const auto id_before = id;

        id = 0; // invalidate

        /* expand bound_args tuple into Trigger() function args */
        const bool result = std::apply([&]<class ...Args> (Args ... args) -> bool {
            return group->Trigger(id_before, std::forward<Args>(args)...);
        }, bound_args);

        group->Remove(id_before);

        group = nullptr;
        bound_args = {};

        return result;
    }
};

} // namespace hyperion

#endif
