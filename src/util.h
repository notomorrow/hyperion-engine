#ifndef UTIL_H
#define UTIL_H

#include "util/string_util.h"

#include <string>
#include <sstream>
#include <cctype>

namespace hyperion {

using BitFlags_t = uint64_t;

#define STR(s) #s
#define STRINGIFY(s) STR(s)

#define _print_assert(mode, cond) { const char *s = "*** " mode " assertion failed ***\n\t" #cond " evaluated to FALSE in file " __FILE__; puts(s); }
#define _print_assert_msg(mode, cond, msg) { const char *s = "*** " mode " assertion failed: " #msg " ***\n\t" #cond " evaluated to FALSE in file " __FILE__; puts(s); }

#define ex_assert_msg(cond, msg) { if (!(cond)) { throw std::runtime_error("Invalid argument: " msg "\n\tCondition: " #cond " evaluated to FALSE in file " __FILE__); } }
#define ex_assert(cond) { if (!(cond)) { const char *s = "*** assertion failed ***\n\t" #cond " evaluated to FALSE in file " __FILE__; throw std::runtime_error(s); } }
#define soft_assert(cond) { if (!(cond)) { _print_assert("soft", cond); return; } }
#define soft_assert_msg(cond, msg) { if (!(cond)) { _print_assert_msg("soft", cond, msg); return; } }
#define soft_assert_return(cond, value) { if (!(cond)) { _print_assert("soft", cond); return value; } }
#define soft_assert_return_msg(cond, msg, value) { if (!(cond)) { _print_assert_msg("soft", cond, msg); return (value); } }
#define soft_assert_break(cond) { if (!(cond)) { _print_assert("soft", cond); break; } }
#define soft_assert_break_msg(cond, msg) { if (!(cond)) { _print_assert_msg("soft", cond, msg); break; } }
#define hard_assert(cond) { if (!(cond)) { _print_assert("hard", cond); exit(1); } }
#define hard_assert_msg(cond, msg) { if (!(cond)) { _print_assert_msg("hard", cond, msg); exit(1); } }
#define soft_assert_continue(cond) { if (!(cond)) { _print_assert("soft", cond); continue; } }
#define soft_assert_continue_msg(cond, msg) { if (!(cond)) { _print_assert_msg("soft", cond, msg); continue; } }

#define not_implemented hard_assert_msg(0, __func__ " not implemented")
#define function_body_not_implemented { not_implemented; }
#define unexpected_value(value) hard_assert_msg(0, STRINGIFY(value) ": unexpected value")
#define unexpected_value_msg(value, msg) hard_assert_msg(0, STRINGIFY(value) ": " #msg)


} // namespace hyperion

#endif
