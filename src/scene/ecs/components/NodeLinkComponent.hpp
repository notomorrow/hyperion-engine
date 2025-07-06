/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Handle.hpp>

#include <HashCode.hpp>

namespace hyperion {

class Node;

HYP_STRUCT(Component, Size = 8, Serialize = false, Editor = false)

struct NodeLinkComponent
{
    HYP_FIELD()
    WeakHandle<Node> node;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode();
    }
};

} // namespace hyperion
