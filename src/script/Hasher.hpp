#ifndef HYPERION_HASHER_HPP
#define HYPERION_HASHER_HPP

#include <core/Types.hpp>

namespace hyperion {

using HashFNV1 = uint32;

constexpr HashFNV1 hash_fnv_1(const char *str)
{
    constexpr uint32 prime = 16777619u;
    constexpr uint32 offset_basis = 2166136261u;

    uint32 hash = offset_basis;

    char c = 0;

    while ((c = *str++)) {
        hash *= prime;
        hash ^= c;
    }

    return hash;
}

} // namespace hyperion

#endif