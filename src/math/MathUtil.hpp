#ifndef MATHUTIL_H
#define MATHUTIL_H

#include "Vector2.hpp"
#include "Vector3.hpp"
#include "Vector4.hpp"

#include <util/Defines.hpp>
#include <Types.hpp>

#include <math.h>
#include <cstdlib>
#include <cstddef>
#include <cmath>
#include <cfloat>
#include <limits>
#include <type_traits>

namespace hyperion {

static inline Vector2 Vector(float x, float y)
{
    return { x, y };
}

static inline Vector2 Vector(const Vector2 &vec)
{
    return Vector2(vec);
}

static inline Vector3 Vector(float x, float y, float z)
{
    return { x, y, z };
}

static inline Vector3 Vector(const Vector3 &vec)
{
    return Vector3(vec);
}

static inline Vector3 Vector(const Vector2 &xy, float z)
{
    return Vector3(xy.x, xy.y, z);
}

static inline Vector4 Vector(float x, float y, float z, float w)
{
    return { x, y, z, w };
}

static inline Vector4 Vector(const Vector4 &vec)
{
    return Vector4(vec);
}

static inline Vector4 Vector(const Vector2 &xy, const Vector2 &zw)
{
    return Vector4(xy.x, xy.y, zw.x, zw.y);
}

static inline Vector4 Vector(const Vector2 &xy, float z, float w)
{
    return Vector4(xy.x, xy.y, z, w);
}

static inline Vector4 Vector(const Vector3 &xyz, float w)
{
    return Vector4(xyz.x, xyz.y, xyz.z, w);
}

template <class T>
constexpr bool is_math_vector_v = std::is_base_of_v<T, Vector2>
                               || std::is_base_of_v<T, Vector3>
                               || std::is_base_of_v<T, Vector4>;

class MathUtil {
    static UInt64 g_seed;

public:
    template <class T>
    static constexpr T pi = T(3.14159265358979);

    template <class T>
    static constexpr T epsilon = T(FLT_EPSILON);

    template<>
    static constexpr float epsilon<float> = FLT_EPSILON;

    template<>
    static constexpr float epsilon<double> = DBL_EPSILON;

    template <class T>
    static constexpr HYP_ENABLE_IF(is_math_vector_v<T>, T) MaxSafeValue()
        { return T(MaxSafeValue<std::remove_all_extents_t<decltype(T::values)>>()); }

    template <class T>
    static constexpr HYP_ENABLE_IF(is_math_vector_v<T>, T) MinSafeValue()
        { return T(MinSafeValue<std::remove_all_extents_t<decltype(T::values)>>()); }
    
    template <class T>
    static constexpr HYP_ENABLE_IF(std::is_enum_v<T> && !is_math_vector_v<T>, std::underlying_type_t<T>) MaxSafeValue()
        { return std::numeric_limits<std::underlying_type_t<T>>::max(); }

    template <class T>
    static constexpr HYP_ENABLE_IF(std::is_enum_v<T> && !is_math_vector_v<T>, std::underlying_type_t<T>) MinSafeValue()
        { return std::numeric_limits<std::underlying_type_t<T>>::lowest(); }

    template <class T>
    static constexpr HYP_ENABLE_IF(!std::is_enum_v<T> && !is_math_vector_v<T>, T) MaxSafeValue()
        { return std::numeric_limits<T>::max(); }

    template <class T>
    static constexpr HYP_ENABLE_IF(!std::is_enum_v<T> && !is_math_vector_v<T>, T) MinSafeValue()
        { return std::numeric_limits<T>::lowest(); }
    
    static Vector2 SafeValue(const Vector2 &value)
        { return Vector2::Max(Vector2::Min(value, MaxSafeValue<decltype(value[0])>()), MinSafeValue<decltype(value[0])>()); }

    static Vector3 SafeValue(const Vector3 &value)
        { return Vector3::Max(Vector3::Min(value, MaxSafeValue<decltype(value[0])>()), MinSafeValue<decltype(value[0])>()); }

    static Vector4 SafeValue(const Vector4 &value)
        { return Vector4::Max(Vector4::Min(value, MaxSafeValue<decltype(value[0])>()), MinSafeValue<decltype(value[0])>()); }

    template <class T>
    static T SafeValue(const T &value)
        { return MathUtil::Max(MathUtil::Min(value, MathUtil::MaxSafeValue<T>()), MathUtil::MinSafeValue<T>()); }

    template <class T>
    static constexpr T NaN()
        { return std::numeric_limits<T>::quiet_NaN(); }

    template <class T>
    static constexpr HYP_ENABLE_IF(is_math_vector_v<T>, bool) ApproxEqual(const T &a, const T &b)
        { return a.DistanceSquared(b) < epsilon<std::remove_all_extents_t<decltype(T::values)>>; }

    template <class T>
    static constexpr HYP_ENABLE_IF(!is_math_vector_v<T>, bool) ApproxEqual(const T &a, const T &b, T eps = epsilon<T>)
        { return Abs(a - b) <= eps; }

    template <class T>
    static HYP_ENABLE_IF(is_math_vector_v<T>, T) RandRange(const T &a, const T &b)
    {
        T result;

        for (UInt i = 0; i < static_cast<UInt>(std::size(result.values)); i++) {
            result.values[i] = RandRange(a.values[i], b.values[i]);
        }

        return result;
    }

    template <class T>
    static HYP_ENABLE_IF(!is_math_vector_v<T>, T) RandRange(const T &a, const T &b)
    {
        const auto random = T(rand()) / T(RAND_MAX);
        const auto diff = b - a;
        const auto r = random * diff;

        return a + r;
    }

    template <class T>
    static constexpr T RadToDeg(const T &rad)
        { return rad * T(180) / pi<T>; }

    template <class T>
    static constexpr T DegToRad(const T &deg)
        { return deg * pi<T> / T(180); }

    template <class T>
    static constexpr T Clamp(const T &val, const T &min, const T &max)
    {
        if (val > max) {
            return max;
        } else if (val < min) {
            return min;
        } else {
            return val;
        }
    }

    template <class T, class U, class V>
    static constexpr auto Lerp(const T &from, const U &to, const V &amt) -> decltype(from + amt * (to - from))
        { return from + amt * (to - from); }

    template <class T>
    static HYP_ENABLE_IF(is_math_vector_v<T>, T) Min(const T &a, const T &b)
        { return T::Min(a, b); }

    template <class T>
    static HYP_ENABLE_IF(is_math_vector_v<T>, T) Max(const T &a, const T &b)
        { return T::Max(a, b); }

    template <class T, class U, class V = std::common_type_t<T, U>>
    static constexpr V Min(T a, U b)
        { return (a < b) ? a : b; }

    template <class T, class U, class V = std::common_type_t<T, U>, class ...Args>
    static constexpr V Min(T a, U b, Args... args)
        { return Min(Min(a, b), args...); }
    
    template <class T, class U, class V = std::common_type_t<T, U>>
    static constexpr V Max(T a, U b)
        { return (a > b) ? a : b; }

    template <class T, class U, class V = std::common_type_t<T, U>, class ...Args>
    static constexpr V Max(T a, U b, Args... args)
        { return Max(Max(a, b), args...); }

    template <class T, class IntegralType = int>
    static constexpr IntegralType Sign(T value)
        { return IntegralType(T(0) < value) - IntegralType(value < T(0)); }

    template <class T, class IntegralType = int>
    static HYP_ENABLE_IF(is_math_vector_v<T>, T) Floor(const T &a)
    {
        T result{}; /* doesn't need initialization but gets rid of annoying warnings */

        for (int i = 0; i < std::size(result.values); i++) {
            result.values[i] = Floor<std::decay_t<decltype(T::values[0])>, IntegralType>(a.values[i]);
        }

        return result;
    }

    template <class T, class IntegralType = int>
    static HYP_ENABLE_IF(is_math_vector_v<T>, T) Ceil(const T &a)
    {
        T result{}; /* doesn't need initialization but gets rid of annoying warnings */

        for (int i = 0; i < std::size(result.values); i++) {
            result.values[i] = Ceil<std::decay_t<decltype(T::values[0])>, IntegralType>(a.values[i]);
        }

        return result;
    }

    template <class T, class IntegralType = int>
    static HYP_ENABLE_IF(!is_math_vector_v<T>, IntegralType) Floor(T a)
        { return IntegralType(std::floor(a)); }

    template <class T, class IntegralType = int>
    static HYP_ENABLE_IF(!is_math_vector_v<T>, IntegralType) Ceil(T a)
        { return IntegralType(std::ceil(a)); }

    template <class T>
    static T Fract(T a) { return a - Floor<T, T>(a); }

    template <class T>
    static T Exp(T a) { std::exp(a); }
    
    template <class T>
    static constexpr HYP_ENABLE_IF(!std::is_floating_point_v<T>, T) Abs(T a) { return std::abs(a); }
    
    template <class T>
    static constexpr HYP_ENABLE_IF(std::is_floating_point_v<T>, T) Abs(T a) { return std::fabs(a); }

    template <class T, class U = T>
    static HYP_ENABLE_IF(!is_math_vector_v<T>, U) Round(T a) { return U(std::round(a)); }

    template <class T>
    static HYP_ENABLE_IF(is_math_vector_v<T>, T) Round(const T &a) { return T::Round(a); }

    static Float Sin(Float x) { return sinf(x); }
    static Double Sin(Double x) { return sin(x); }
    static Float Arcsin(Float x) { return asinf(x); }
    static Double Arcsin(Double x) { return asin(x); }

    static Float Cos(Float x) { return cosf(x); }
    static Double Cos(Double x) { return cos(x); }
    static Float Arccos(Float x) { return acosf(x); }
    static Double Arccos(Double x) { return acos(x); }

    static Float Tan(Float x) { return tanf(x); }
    static Double Tan(Double x) { return tan(x); }
    static Float Arctan(Float x) { return atanf(x); }
    static Double Arctan(Double x) { return atan(x); }

    template <class T, class U = T, class V = U>
    static constexpr bool InRange(T value, const std::pair<U, V> &range)
        { return value >= range.first && value < range.second; }

    template <class T, class U = T>
    static constexpr U Sqrt(T value)
    {
        if constexpr (std::is_same_v<U, double>) {
            return sqrt(static_cast<double>(value));
        } else if constexpr (std::is_same_v<U, float>) {
            return sqrtf(static_cast<float>(value));
        } else {
            return static_cast<U>(sqrtf(static_cast<float>(value)));
        }
    }

    template <class T>
    static constexpr bool IsPowerOfTwo(T value)
        { return (value & (value - 1)) == 0; }

    /* Fast log2 for power-of-2 numbers */
    static UInt64 FastLog2_Pow2(UInt64 value)
    {
#if defined(__clang__) || defined(__GNUC__)
    #if defined(_MSC_VER)
        return FastLog2(value); // fallback :/
    #else
        return __builtin_ctzll(value);
    #endif
#elif defined(_MSC_VER)
        return _tzcnt_u64(value);
#else
        return __builtin_ctzll(value);
#endif
    }

    // https://stackoverflow.com/questions/11376288/fast-computing-of-log2-for-64-bit-integers
    static inline constexpr UInt64 FastLog2(UInt64 value)
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

        return tab64[(((static_cast<UInt64>(value) - (value >> 1)) * 0x07EDD5E59A4E28C2)) >> 58];
    }

    // https://www.techiedelight.com/round-next-highest-power-2/
    static inline constexpr UInt64 NextPowerOf2(UInt64 value)
    {
        // decrement `n` (to handle the case when `n` itself
        // is a power of 2)
        value = value - 1;

        // next power of two will have a bit set at position `lg+1`.
        return 1ull << (FastLog2(value) + 1);
    }

    static inline constexpr UInt64 PreviousPowerOf2(UInt64 value)
    {
        // dumb implementation
        UInt64 result = 1;

        while (result * 2 < value) {
            result *= 2;
        }

        return result;

        //return NextPowerOf2(value) / 2;
    }

    template <class T, class U>
    static inline constexpr auto NextMultiple(T &&value, U &&multiple) -> std::common_type_t<T, U>
    {
        if (multiple == 0) {
            return value;
        }

        auto remainder = value % multiple;

        if (remainder == 0) {
            return value;
        }

        return value + multiple - remainder;
    }

    template <class T>
    static inline auto FastRand()
    {
        g_seed = (214013 * g_seed + 2531011); 

        return (g_seed >> 16) & 0x7FFF; 
    }
};

} // namespace hyperion

#endif
