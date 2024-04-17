/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_BLAS_COMPONENT_HPP
#define HYPERION_ECS_BLAS_COMPONENT_HPP

#include <core/Handle.hpp>
#include <rendering/rt/BLAS.hpp>
#include <HashCode.hpp>

namespace hyperion {

struct BLASComponent
{
    Handle<BLAS>    blas;
    
    HashCode        transform_hash_code;
};

} // namespace hyperion

#endif