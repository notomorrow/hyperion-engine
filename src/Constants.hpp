/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <Types.hpp>

#include <type_traits>
#include <utility>

namespace hyperion {

#if !defined(HYP_VERSION_MAJOR) || !defined(HYP_VERSION_MINOR) || !defined(HYP_VERSION_PATCH)

#ifndef HYP_BUILDTOOL
#error "HYP_VERSION_MAJOR, HYP_VERSION_MINOR, and HYP_VERSION_PATCH must be defined"
#endif

// Define to let build continue
#define HYP_VERSION_MAJOR 0
#define HYP_VERSION_MINOR 0
#define HYP_VERSION_PATCH 0

#endif

constexpr uint8 engineMajorVersion = HYP_VERSION_MAJOR;
constexpr uint8 engineMinorVersion = HYP_VERSION_MINOR;
constexpr uint8 enginePatchVersion = HYP_VERSION_PATCH;
constexpr uint32 engineVersion = (engineMajorVersion << 16) | (engineMinorVersion << 8) | enginePatchVersion;
constexpr uint64 engineBinaryMagicNumber = (uint64(0x505948) << 32) | engineVersion;

constexpr uint32 maxFramesInFlight = 2;
constexpr uint32 numAsyncRenderingCommandBuffers = 4;
constexpr uint32 numAsyncComputeCommandBuffers = 1;

constexpr uint32 maxBoundReflectionProbes = 16;
constexpr uint32 maxBoundAmbientProbes = 4096;
constexpr uint32 maxBoundPointShadowMaps = 16;
constexpr uint32 maxBoundEnvironmentMaps = 1;
constexpr uint32 maxBoundTextures = 16;

constexpr uint32 maxBindlessResources = 4096;

constexpr uint32 numGbufferTargets = 8;

template <class... T>
constexpr bool resolutionFailure = false;

template <class T>
using NormalizedType = std::conditional_t<std::is_function_v<T>, std::add_pointer_t<T>, std::remove_cvref_t<T>>;

template <class T>
constexpr bool isPodType = std::is_standard_layout_v<T>
    && std::is_trivially_copyable_v<T>
    && std::is_trivially_copy_assignable_v<T>
    && std::is_trivially_move_constructible_v<T>
    && std::is_trivially_move_assignable_v<T>
    && std::is_trivially_destructible_v<T>;

template <class T, SizeType = sizeof(T)>
std::true_type implementationExistsImpl(T*);

std::false_type implementationExistsImpl(...);

template <class T>
constexpr bool implementationExists = decltype(implementationExistsImpl(std::declval<T*>()))::value;

template <class T>
struct HandleDefinition;

template <class T>
constexpr bool isConstPointer = std::is_pointer_v<T> && std::is_const_v<std::remove_pointer_t<T>>;
template <class T>
using RemoveConstPointer = std::add_pointer_t<std::remove_const_t<std::remove_pointer_t<T>>>;

static inline bool IsBigEndian()
{
    constexpr union
    {
        uint32 i;
        uint8 ch[sizeof(uint32)];
    } u = { 0x01020304 };

    return u.ch[0] == 1;
}

static inline bool IsLittleEndian()
{
    return !IsBigEndian();
}

constexpr uint8 SwapEndianness(uint8 value)
{
    return value;
}

constexpr uint16 SwapEndianness(uint16 value)
{
    return uint16(((value >> 8u) & 0xffu) | ((value & 0xffu) << 8u));
}

constexpr uint32 SwapEndianness(uint32 value)
{
    return ((value & 0xff000000u) >> 24u) | ((value & 0x00ff0000u) >> 8u) | ((value & 0x0000ff00u) << 8u) | ((value & 0x000000ffu) << 24u);
}

constexpr uint64 SwapEndianness(uint64 value)
{
    return ((value & 0xff00000000000000ull) >> 56ull) | ((value & 0x00ff000000000000ull) >> 40ull) | ((value & 0x0000ff0000000000ull) >> 24ull) | ((value & 0x000000ff00000000ull) >> 8ull) | ((value & 0x00000000ff000000ull) << 8ull) | ((value & 0x0000000000ff0000ull) << 24ull) | ((value & 0x000000000000ff00ull) << 40ull) | ((value & 0x00000000000000ffull) << 56ull);
}

constexpr int8 SwapEndianness(int8 value)
{
    return value;
}

constexpr int16 SwapEndianness(int16 value)
{
    union
    {
        uint16 u;
        int16 i;
    };

    i = value;

    u = SwapEndianness(u);

    return i;
}

constexpr int32 SwapEndianness(int32 value)
{
    union
    {
        uint32 u;
        int32 i;
    };

    i = value;

    u = SwapEndianness(u);

    return i;
}

constexpr int64 SwapEndianness(int64 value)
{
    union
    {
        uint64 u;
        int64 i;
    };

    i = value;

    u = SwapEndianness(u);

    return i;
}

} // namespace hyperion
