#ifndef MATHUTIL_H
#define MATHUTIL_H

namespace apex {
class MathUtil {
public:
    const static double PI;
    const static double epsilon;

    template <typename T>
    static inline T RadToDeg(const T rad)
    {
        return rad * 180.0f / PI;
    }

    template <typename T>
    static inline T DegToRad(const T deg)
    {
        return deg * PI / 180.0f;
    }

    template <typename T>
    static inline T Clamp(const T val, const T min, const T max)
    {
        if (val > max) return max;
        else if (val < min) return min;
        else return val;
    }

    template <typename T>
    static inline T Lerp(const T from, const T to, const T amt)
    {
        return from + amt * (to - from);
    }

    template <typename T>
    static inline T Min(const T a, const T b)
    {
        if (a < b) return a;
        else return b;
    }

    template <typename T>
    static inline T Max(const T a, const T b)
    {
        if (a > b) return a;
        else return b;
    }
};
}
#endif