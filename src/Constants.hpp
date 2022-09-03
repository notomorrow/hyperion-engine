#ifndef HYPERION_CONSTANTS_H
#define HYPERION_CONSTANTS_H

#include <Types.hpp>

#include <type_traits>
#include <utility>

namespace hyperion {

constexpr UInt max_frames_in_flight = 2;
constexpr UInt max_bound_env_probes = 1; // temp

constexpr UInt num_gbuffer_textures = 5;

template <class ...T>
constexpr bool resolution_failure = false;

template <class T>
using NormalizedType = std::remove_cv_t<std::decay_t<T>>;

template <char ... Chars>
using character_sequence = std::integer_sequence<char, Chars...>;

template <class T, T... Chars>
constexpr character_sequence<Chars...> MakeCharSequence(T &&) { return { }; }

template <class T, std::size_t = sizeof(T)>
std::true_type implementation_exists_impl(T *);

std::false_type implementation_exists_impl(...);

template <class T>
constexpr bool implementation_exists = decltype(implementation_exists_impl(std::declval<T*>()))::value;

} // namespace hyperion

#endif
