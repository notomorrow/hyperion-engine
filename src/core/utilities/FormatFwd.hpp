#ifndef HYPERION_FORMAT_FWD_HPP
#define HYPERION_FORMAT_FWD_HPP

// Forward declaration for Formatter struct (for specialization)

namespace hyperion {

// forward decl for string
namespace containers {
namespace detail {

template <int StringType>
class String;

} // namespace detail
} // namespace containers

namespace utilities {

template <int StringType, class T, typename T2 = void>
struct Formatter;

} // namespace utilities
} // namespace hyperion

#endif