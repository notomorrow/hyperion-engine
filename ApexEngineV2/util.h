#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <sstream>

namespace apex {

#define STR(s) #s
#define STRINGIFY(s) STR(s)

#define _print_assert(mode, cond) { const char *s = "*** " #mode " assertion failed ***\n\t" #cond " evaluated to FALSE in file " __FILE__; puts(s); }
#define _print_assert_msg(mode, cond, msg) { const char *s = "*** " #mode " assertion failed: " #msg " ***\n\t" #cond " evaluated to FALSE in file " __FILE__; puts(s); }

#define ex_assert_msg(cond, msg) { if (!(cond)) { throw std::runtime_error("Invalid argument: " msg "\n\tCondition: " #cond " evaluated to FALSE in file " __FILE__ " on line " __LINE__ "."); } }
#define ex_assert(cond) { if (!(cond)) { const char *s = "*** assertion failed ***\n\t" #cond " evaluated to FALSE in file " __FILE__; throw std::runtime_error(s); } }
#define soft_assert(cond) { if (!(cond)) { _print_assert("soft", cond); return; } }
#define soft_assert_msg(cond, msg) { if (!(cond)) { _print_assert_msg("soft", cond, msg); return; } }
#define hard_assert(cond) { if (!(cond)) { _print_assert("hard", cond); exit(1); } }
#define hard_assert_msg(cond, msg) { if (!(cond)) { _print_assert_msg("hard", cond, msg); exit(1); } }

#define not_implemented hard_assert_msg(0, __func__ " not implemented")
#define function_body_not_implemented { not_implemented; }
#define unexpected_value(value) hard_assert_msg(0, STRINGIFY(value) ": unexpected value")
#define unexpected_value_msg(value, msg) hard_assert_msg(0, STRINGIFY(value) ": " #msg)

} // namespace apex

#endif
