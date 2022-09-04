#ifndef HYPERION_IMAGE_UTIL_H
#define HYPERION_IMAGE_UTIL_H

#include <math/MathUtil.hpp>
#include <Types.hpp>

namespace hyperion {

class ImageUtil {
public:
    static inline
    void ConvertBPP(
        UInt width, UInt height, UInt depth,
        UInt8 in_bpp, UInt8 out_bpp,
        const UByte * const in_bytes,
        UByte *out_bytes
    )
    {
        const auto min_bpp = MathUtil::Min(in_bpp, out_bpp);

        for (UInt x = 0; x < width; x++) {
            for (UInt y = 0; y < height; y++) {
                for (UInt z = 0; z < depth; z++) {
                    const UInt idx = x * height * depth + y * depth + z;
                    const UInt in_index = idx * in_bpp;
                    const UInt out_index = idx * out_bpp;

                    for (UInt8 i = 0; i < min_bpp; i++) {
                        out_bytes[out_index + i] = in_bytes[in_index + i];
                    }

                    for (UInt8 i = min_bpp; i < out_bpp; i++) {
                        out_bytes[out_index + i] = 255;
                    }
                }
            }
        }
    }
};

} // namespace hyperion

#endif
