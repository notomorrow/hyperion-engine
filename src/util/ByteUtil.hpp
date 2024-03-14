#ifndef HYPERION_BYTE_UTIL_H
#define HYPERION_BYTE_UTIL_H

#include <math/MathUtil.hpp>
#include <math/Vector4.hpp>

#include <Types.hpp>

namespace hyperion {

class ByteUtil
{
public:
    /**
     * \brief Pack a 32-bit float as a 16-bit float in the higher 16 bits of a uint32.
     */
    static inline uint32 PackFloat16(float value)
    {
        union {
            uint32 u;
            float f;
        };

        f = value * 65535.0f;

        return u << 16;
    }

    static inline float UnpackFloat16(uint32 value)
    {
        union {
            uint32 u;
            float f;
        };

        u = value >> 16;

        return f / 65535.0f;
    }

    static inline uint32 PackFloat(float value)
    {
        union {
            uint32 u;
            float f;
        };

        f = value;

        return u;
    }

    static inline float UnpackFloat(uint32 value)
    {
        union {
            uint32 u;
            float f;
        };

        u = value;

        return f;
    }

    static inline uint32 PackVec4f(const Vector4 &vec)
    {
        union {
            uint8 bytes[4];
            uint32 result;
        };

        bytes[0] = MathUtil::Round<float, uint8>(MathUtil::Clamp(vec.values[0], 0.0f, 1.0f) * 255.0f);
        bytes[1] = MathUtil::Round<float, uint8>(MathUtil::Clamp(vec.values[1], 0.0f, 1.0f) * 255.0f);
        bytes[2] = MathUtil::Round<float, uint8>(MathUtil::Clamp(vec.values[2], 0.0f, 1.0f) * 255.0f);
        bytes[3] = MathUtil::Round<float, uint8>(MathUtil::Clamp(vec.values[3], 0.0f, 1.0f) * 255.0f);

        return result;
    }

    static inline Vec4f UnpackVec4f(uint32 value)
    {
        return {
            float(value & 0xff000000) / 255.0f,
            float(value & 0x00ff0000) / 255.0f,
            float(value & 0x0000ff00) / 255.0f,
            float(value & 0x000000ff) / 255.0f
        };
    }

    template <class T>
    static inline constexpr T AlignAs(T value, uint alignment)
    {
        return ((value + alignment - 1) / alignment) * alignment;
    }

    static uint LowestSetBitIndex(uint64 bits);
};

/*! \brief Converts a value of type \ref{To} to a value of type \ref{From}.
 *  Both types must be standard layout and have the same size.
 *  \tparam To The type to convert to.
 *  \tparam From The type to convert from.
 *  \param from The value to convert.
 *  \return The value of type \ref{From} converted to type \ref{To}.
 */
template <class To, class From>
static HYP_FORCE_INLINE To BitCast(const From &from)
{
    return ValueStorage<To>(&from).Get();
}

#define FOR_EACH_BIT(_num, _iter) for (uint64 num = (_num), _iter = ByteUtil::LowestSetBitIndex(num); _iter != uint(-1); num &= ~_iter)

} // namespace hyperion

#endif