/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_BLAS_COMPONENT_HPP
#define HYPERION_ECS_BLAS_COMPONENT_HPP

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/rt/RendererAccelerationStructure.hpp>

#include <HashCode.hpp>

namespace hyperion {

HYP_STRUCT(Component, Editor=false)
struct BLASComponent
{
    HYP_FIELD()
    BLASRef     blas;
    
    HYP_FIELD()
    HashCode    transform_hash_code;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode();
    }
};

} // namespace hyperion

#endif