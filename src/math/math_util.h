#ifndef MATHUTIL_H
#define MATHUTIL_H

#include "vector2.h"
#include "vector3.h"
#include "vector4.h"

#include <cstdlib>
#include <cstddef>
#include <cmath>
#include <limits>
#include <type_traits>

namespace hyperion {

class MathUtil {
public:
    static constexpr float pi = 3.14159265358979f;
    static constexpr float epsilon = 0.0001f;

    template <typename T>
    static inline constexpr typename std::enable_if_t<std::is_enum_v<T>, std::underlying_type_t<T>>
    MaxSafeValue() { return std::numeric_limits<std::underlying_type_t<T>>::max(); }

    template <typename T>
    static inline constexpr typename std::enable_if_t<std::is_enum_v<T>, std::underlying_type_t<T>>
    MinSafeValue() { return std::numeric_limits<std::underlying_type_t<T>>::lowest(); }

    template <typename T>
    static inline constexpr typename std::enable_if_t<!std::is_enum_v<T>, T>
    MaxSafeValue() { return std::numeric_limits<T>::max(); }

    template <typename T>
    static inline constexpr typename std::enable_if_t<!std::is_enum_v<T>, T>
    MinSafeValue() { return std::numeric_limits<T>::lowest(); }


    static inline Vector2 SafeValue(const Vector2 &value)
        { return Vector2::Max(Vector2::Min(value, MaxSafeValue<decltype(value[0])>()), MinSafeValue<decltype(value[0])>()); }

    static inline Vector3 SafeValue(const Vector3 &value)
        { return Vector3::Max(Vector3::Min(value, MaxSafeValue<decltype(value[0])>()), MinSafeValue<decltype(value[0])>()); }

    static inline Vector4 SafeValue(const Vector4 &value)
        { return Vector4::Max(Vector4::Min(value, MaxSafeValue<decltype(value[0])>()), MinSafeValue<decltype(value[0])>()); }

    template <typename T>
    static inline T SafeValue(const T &value)
        { return MathUtil::Max(MathUtil::Min(value, MathUtil::MaxSafeValue<T>()), MathUtil::MinSafeValue<T>()); }

    template <typename T>
    static inline constexpr T NaN()
        { return std::numeric_limits<T>::quiet_NaN(); }

    template <typename T>
    static inline constexpr bool Approximately(const T &a, const T &b)
        { return std::abs(a - b) <= T(epsilon); }

    template <typename T>
    static inline T Random(const T &a, const T &b)
    {
        T random = T(rand()) / T(RAND_MAX);
        T diff = b - a;
        T r = random * diff;
        return a + r;
    }

    template <typename T>
    static inline constexpr T RadToDeg(const T &rad)
        { return rad * T(180) / T(pi); }

    template <typename T>
    static inline constexpr T DegToRad(const T &deg)
        { return deg * T(pi) / T(180); }

    template <typename T>
    static inline constexpr T Clamp(const T &val, const T &min, const T &max)
    {
        if (val > max) {
            return max;
        } else if (val < min) {
            return min;
        } else {
            return val;
        }
    }

    template <typename T>
    static inline constexpr T Lerp(const T &from, const T &to, const T &amt)
        { return from + amt * (to - from); }

    template <typename T>
    static inline T Fract(T f)
        { return f - floorf(f); }

    template <typename T>
    static inline constexpr T Min(const T &a, const T &b)
        { return (a < b) ? a : b; }

    template <typename T>
    static inline constexpr T Max(T a, T b)
        { return (a > b) ? a : b; }

    template <typename T, typename... Args>
    static inline constexpr T Max(T a, T b, Args... args)
        { return Max(Max(a, b), args...); }

    template <typename T, typename SignedType = int>
    static inline constexpr SignedType Sign(const T &value)
        { return SignedType(T(0) < value) - SignedType(value < T(0)); }

    template <typename T, typename SignedType = int>
    static inline SignedType Floor(T a)
        { return SignedType(std::floor(a)); }

    template <typename T, typename SignedType = int>
    static inline SignedType Ceil(T a)
        { return SignedType(std::ceil(a)); }

    template <typename T>
    static inline T Exp(T a)
        { return std::exp(a); }

    template <typename T>
    static inline T Round(T a)
        { return std::round(a); }

    template <typename T, typename U = T, typename V = U>
    static inline constexpr bool InRange(T value, const std::pair<U, V> &range)
        { return value >= range.first && value < range.second; }

    static inline constexpr bool IsPowerOfTwo(uint64_t value)
        { return (value & (value - 1)) == 0; }

    /* Fast log2 for power-of-2 numbers */
    static inline uint64_t FastLog2_Pow2(uint64_t value)
    {
#ifdef _MSC_VER
        return _tzcnt_u64(value);
#else
        return __builtin_ctzll(value);
#endif
    }

    // https://stackoverflow.com/questions/11376288/fast-computing-of-log2-for-64-bit-integers
    static inline constexpr uint64_t FastLog2(uint64_t value)
    {
        const int tab64[64] = {
            63,  0, 58,  1, 59, 47, 53,  2,
            60, 39, 48, 27, 54, 33, 42,  3,
            61, 51, 37, 40, 49, 18, 28, 20,
            55, 30, 34, 11, 43, 14, 22,  4,
            62, 57, 46, 52, 38, 26, 32, 41,
            50, 36, 17, 19, 29, 10, 13, 21,
            56, 45, 25, 31, 35, 16,  9, 12,
            44, 24, 15,  8, 23,  7,  6,  5
        };

        value |= value >> 1;
        value |= value >> 2;
        value |= value >> 4;
        value |= value >> 8;
        value |= value >> 16;
        value |= value >> 32;

        return tab64[(((static_cast<uint64_t>(value) - (value >> 1)) * 0x07EDD5E59A4E28C2)) >> 58];
    }

    // https://www.techiedelight.com/round-next-highest-power-2/
    static inline constexpr uint64_t NextPowerOf2(uint64_t value)
    {
        // decrement `n` (to handle the case when `n` itself
        // is a power of 2)
        value = value - 1;

        // next power of two will have a bit set at position `lg+1`.
        return 1ull << (FastLog2(value) + 1);
    }

    static inline uint64_t NextMultiple(uint64_t value, uint64_t multiple)
    {
        if (multiple == 0) {
            return value;
        }

        uint64_t remainder = value % multiple;

        if (remainder == 0) {
            return value;
        }

        return value + multiple - remainder;
    }
};

} // namespace hyperion

#endif
