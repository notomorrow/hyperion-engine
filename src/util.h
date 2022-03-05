#ifndef UTIL_H
#define UTIL_H

#include "util/string_util.h"

#include <string>
#include <sstream>
#include <cctype>

#include "system/debug.h"

#ifdef __GNUC__
#define HYPERION_PACK_BEGIN
#define HYPERION_PACK_END __attribute__((__packed__));
#endif

#ifdef _MSC_VER
#define HYPERION_PACK_BEGIN __pragma( pack(push, 1) )
#define HYPERION_PACK_END ;__pragma( pack(pop))
#endif

namespace hyperion {

using BitFlags_t = uint64_t;

} // namespace hyperion

#endif
