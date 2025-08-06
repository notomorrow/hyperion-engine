/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/math/Transform.hpp>

#include <core/HashCode.hpp>

namespace hyperion {

HYP_STRUCT(Component, Label = "Transform Component", Description = "Controls the translation, rotation, and scale of an object.", Editor = false)
struct TransformComponent
{
    HYP_FIELD(Property = "Transform", Serialize = true)
    Transform transform;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hashCode;

        hashCode.Add(transform);

        return hashCode;
    }
};

static_assert(sizeof(TransformComponent) == 112, "TransformComponent must be 112 bytes to match C# struct size");

} // namespace hyperion
