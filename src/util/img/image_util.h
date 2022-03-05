#ifndef HYPERION_IMAGE_UTIL_H
#define HYPERION_IMAGE_UTIL_H

#include <math/math_util.h>
#include <types.h>

namespace hyperion {

class ImageUtil {
public:
    static inline
        void ConvertBpp(
            size_t width, size_t height,
            ubyte in_bpp, ubyte out_bpp,
            const ubyte *const in_bytes,
            ubyte *out_bytes)
    {
        const ubyte min_bpp = MathUtil::Min(in_bpp, out_bpp);

        for (size_t x = 0; x < width; x++) {
            for (size_t y = 0; y < height; y++) {
                size_t in_index = (x * width + y) * in_bpp;
                size_t out_index = (x * width + y) * out_bpp;

                for (ubyte i = 0; i < min_bpp; i++) {
                    out_bytes[out_index + i] = in_bytes[in_index + i];
                }
                for (ubyte i = min_bpp; i < out_bpp; i++) {
                    out_bytes[out_index + i] = 255;
                }
            }
        }
    }
};

} // namespace hyperion

#endif