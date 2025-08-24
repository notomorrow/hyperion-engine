#pragma once

#include <core/Types.hpp>

namespace hyperion {

using HashFNV1 = uint32;

constexpr HashFNV1 hashFnv1(const char* str)
{
    constexpr uint32 prime = 16777619u;
    constexpr uint32 offsetBasis = 2166136261u;

    uint32 hash = offsetBasis;

    char c = 0;

    while ((c = *str++))
    {
        hash *= prime;
        hash ^= c;
    }

    return hash;
}

} // namespace hyperion

