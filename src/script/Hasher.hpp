#ifndef HASHER_HPP
#define HASHER_HPP

#include <Types.hpp>

namespace hyperion {

using HashFNV1 = UInt32;

constexpr HashFNV1 hash_fnv_1(const char *str)
{
    constexpr UInt32 PRIME = 16777619u;
    constexpr UInt32 OFFSET_BASIS = 2166136261u;

    UInt32 hash = OFFSET_BASIS;

    char c = 0;

    while ((c = *str++)) {
        hash *= PRIME;
        hash ^= c;
    }

    return hash;
}

} // namespace hyperion

#endif