#ifndef UTIL_H
#define UTIL_H

#include "util/string_util.h"

#include <string>
#include <sstream>
#include <cctype>

#include "system/debug.h"

#ifdef __GNUC__
#define HYP_PACK_BEGIN
#define HYP_PACK_END __attribute__((__packed__));
#define HYP_FORCE_INLINE __attribute__((always_inline))
#endif

#ifdef _MSC_VER
#define HYP_PACK_BEGIN __pragma( pack(push, 1) )
#define HYP_PACK_END ;__pragma( pack(pop))
#define HYP_FORCE_INLINE __forceinline
#endif

namespace hyperion {

using BitFlags_t = uint64_t;

} // namespace hyperion

#endif
