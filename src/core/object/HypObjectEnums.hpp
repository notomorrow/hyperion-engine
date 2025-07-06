#pragma once
#include <Types.hpp>

namespace hyperion {

enum class HypClassAllocationMethod : uint8
{
    INVALID = uint8(-1),

    NONE = 0,
    HANDLE = 1,
    REF_COUNTED_PTR = 2
};

} // namespace hyperion
