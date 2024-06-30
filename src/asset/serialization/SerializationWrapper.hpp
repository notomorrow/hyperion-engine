/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SERIALIZATION_WRAPPER_HPP
#define HYPERION_SERIALIZATION_WRAPPER_HPP

#include <core/Defines.hpp>

#include <core/Handle.hpp>
#include <core/memory/RefCountedPtr.hpp>

namespace hyperion {

class Node;
class NodeProxy;

template <class T>
struct SerializationWrapper
{
    using Type = std::conditional_t<
        has_opaque_handle_defined<T>,
        Handle<T>,
        T
    >;

    template <class Ty> // use template param to disambiguate const v.s non-const ref
    static auto &&Unwrap(Ty &&value)
    {
        if constexpr (has_opaque_handle_defined<T>) {
            AssertThrow(value.IsValid());

            return *value;
        } else {
            return value;
        }
    }

    static void OnPostLoad(Type &value) { }
};

template <class T>
struct SerializationWrapperReverseMapping { using Type = T; };

template <class T>
struct SerializationWrapperReverseMapping<Handle<T>> { using Type = T; };

template <class T>
struct SerializationWrapper<RC<T>>
{
    using Type = RC<T>;

    static const T &Unwrap(const Type &value)
    {
        AssertThrow(value != nullptr);

        return *value;
    }

    static void OnPostLoad(Type &value) { }
};

template <class T>
struct SerializationWrapperReverseMapping<RC<T>> { using Type = T; };

template <>
struct SerializationWrapper<Node>
{
    using Type = NodeProxy;

    HYP_API static Node &Unwrap(const Type &value);

    HYP_API static void OnPostLoad(Type &value);
};

template <>
struct SerializationWrapperReverseMapping<NodeProxy> { using Type = Node; };

} // namespace hyperion

#endif