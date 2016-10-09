#ifndef MATHUTIL_H
#define MATHUTIL_H

#include <cstdlib>

namespace apex {

class MathUtil {
public:
    const static double PI;
    const static double EPSILON;

    template <typename T>
    static inline T Random(const T &a, const T &b)
    {
        T random = ((T)rand()) / (T)RAND_MAX;
        T diff = b - a;
        T r = random * diff;
        return a + r;
    }

    template <typename T>
    static inline T RadToDeg(const T &rad)
    {
        return rad * (T)180 / (T)PI;
    }

    template <typename T>
    static inline T DegToRad(const T &deg)
    {
        return deg * (T)PI / (T)180;
    }

    template <typename T>
    static inline T Clamp(const T &val, const T &min, const T &max)
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
    static inline T Lerp(const T &from, const T &to, const T &amt)
    {
        return from + amt * (to - from);
    }

    template <typename T>
    static inline T Min(const T &a, const T &b)
    {
        if (a < b) { 
            return a; 
        } else { 
            return b; 
        }
    }

    template <typename T>
    static inline T Max(const T &a, const T &b)
    {
        if (a > b) { 
            return a; 
        } else {
            return b;
        }
    }
};

} // namespace apex

#endif