/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_BOUNDING_BOX_COMPONENT_HPP
#define HYPERION_ECS_BOUNDING_BOX_COMPONENT_HPP

#include <math/BoundingBox.hpp>
#include <HashCode.hpp>

namespace hyperion {

struct BoundingBoxComponent
{
    BoundingBox local_aabb;
    BoundingBox world_aabb;

    HashCode    transform_hash_code;
};

} // namespace hyperion

#endif