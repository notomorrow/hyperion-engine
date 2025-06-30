/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_BOUNDING_BOX_COMPONENT_HPP
#define HYPERION_ECS_BOUNDING_BOX_COMPONENT_HPP

#include <core/math/BoundingBox.hpp>
#include <HashCode.hpp>

namespace hyperion {

HYP_STRUCT(Component, Size = 64, Editor = false)

struct BoundingBoxComponent
{
    HYP_FIELD(Property = "LocalAABB", Serialize = true)
    BoundingBox localAabb;

    HYP_FIELD(Property = "WorldAABB", Serialize = true)
    BoundingBox worldAabb;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hashCode;

        hashCode.Add(localAabb);
        hashCode.Add(worldAabb);

        return hashCode;
    }
};

} // namespace hyperion

#endif