#ifndef HYPERION_CONSTANTS_H
#define HYPERION_CONSTANTS_H

#include <util/Defines.hpp>
#include <Types.hpp>

#include <type_traits>
#include <utility>

namespace hyperion {

constexpr UInt max_frames_in_flight = 2;
constexpr UInt num_async_rendering_command_buffers = 4;
constexpr UInt num_async_compute_command_buffers = 1;
constexpr UInt max_bound_env_probes = 1; // temp
constexpr UInt num_gbuffer_textures = 5;
constexpr bool use_indexed_array_for_object_data = true;
// perform occlusion culling using indirect draw
constexpr bool use_draw_indirect = true;
constexpr bool use_parallel_rendering = HYP_FEATURES_PARALLEL_RENDERING;

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

static HYP_FORCE_INLINE bool IsBigEndian()
{
    constexpr union { UInt32 i; UInt8 ch[sizeof(UInt32)]; } u = { 0x01020304 };

    return u.ch[0] == 1;
}

static HYP_FORCE_INLINE bool IsLittleEndian() { return !IsBigEndian(); }

constexpr UInt8 SwapEndianness(UInt8 value)
{
    return value;
}

constexpr UInt16 SwapEndianness(UInt16 value)
{
    return UInt16(((value >> 8u) & 0xffu) | ((value & 0xffu) << 8u));
}

constexpr UInt32 SwapEndianness(UInt32 value)
{
    return ((value & 0xff000000u) >> 24u) |
        ((value & 0x00ff0000u) >>  8u) |
        ((value & 0x0000ff00u) <<  8u) |
        ((value & 0x000000ffu) << 24u);
}

constexpr UInt64 SwapEndianness(UInt64 value)
{
    return ((value & 0xff00000000000000ull) >> 56ull) |
        ((value & 0x00ff000000000000ull) >> 40ull) |
        ((value & 0x0000ff0000000000ull) >> 24ull) |
        ((value & 0x000000ff00000000ull) >> 8ull) |
        ((value & 0x00000000ff000000ull) << 8ull) |
        ((value & 0x0000000000ff0000ull) << 24ull) |
        ((value & 0x000000000000ff00ull) << 40ull) |
        ((value & 0x00000000000000ffull) << 56ull);
}

constexpr Int8 SwapEndianness(Int8 value)
{
    return value;
}

constexpr Int16 SwapEndianness(Int16 value)
{
    union { UInt16 u; Int16 i; };
    i = value;

    u = SwapEndianness(u);

    return i;
}

constexpr Int32 SwapEndianness(Int32 value)
{
    union { UInt32 u; Int32 i; };
    i = value;

    u = SwapEndianness(u);

    return i;
}

constexpr Int64 SwapEndianness(Int64 value)
{
    union { UInt64 u; Int64 i; };
    i = value;

    u = SwapEndianness(u);

    return i;
}

} // namespace hyperion

#endif
