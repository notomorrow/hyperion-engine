#ifndef HYPERION_CONSTANTS_H
#define HYPERION_CONSTANTS_H

#include <Types.hpp>

#include <type_traits>

namespace hyperion {

constexpr UInt max_frames_in_flight = 2;

constexpr UInt num_gbuffer_textures = 7;

template <class ...T>
constexpr bool resolution_failure = false;

template <class T, std::size_t = sizeof(T)>
std::true_type implementation_exists_impl(T *);

std::false_type implementation_exists_impl(...);

template <class T>
constexpr bool implementation_exists = decltype(implementation_exists_impl(std::declval<T*>()))::value;

} // namespace hyperion

#endif
