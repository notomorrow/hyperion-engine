/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/math/MathUtil.hpp>
#include <core/Types.hpp>

namespace hyperion {

class ImageUtil
{
public:
    static inline void ConvertBPP(
        uint32 width, uint32 height, uint32 depth,
        uint8 inBpp, uint8 outBpp,
        const ubyte* const inBytes,
        ubyte* outBytes)
    {
        const auto minBpp = MathUtil::Min(inBpp, outBpp);

        for (uint32 x = 0; x < width; x++)
        {
            for (uint32 y = 0; y < height; y++)
            {
                for (uint32 z = 0; z < depth; z++)
                {
                    const uint32 idx = x * height * depth + y * depth + z;
                    const uint32 inIndex = idx * inBpp;
                    const uint32 outIndex = idx * outBpp;

                    for (uint8 i = 0; i < minBpp; i++)
                    {
                        outBytes[outIndex + i] = inBytes[inIndex + i];
                    }

                    for (uint8 i = minBpp; i < outBpp; i++)
                    {
                        outBytes[outIndex + i] = 255;
                    }
                }
            }
        }
    }
};

} // namespace hyperion
