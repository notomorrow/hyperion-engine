#ifndef TYPES_H
#define TYPES_H

#include <cstdint>

namespace hyperion {

using UByte    = uint8_t;
using SByte    = int8_t;

using UInt8    = uint8_t;
using UInt16   = uint16_t;
using UInt32   = uint32_t;
using UInt64   = uint64_t;

using UInt     = UInt32;

using Int8     = int8_t;
using Int16    = int16_t;
using Int32    = int32_t;
using Int64    = int64_t;

using Int      = Int32;

using Float32  = float;
static_assert(sizeof(Float32) == 4, "Expected float to be 32-bit!");

using Float64  = double;
static_assert(sizeof(Float64) == 8, "Expected double to be 64-bit!");

using Float    = Float32;

using SizeType = decltype(sizeof(void *));

} // namespace hyperion

#endif
