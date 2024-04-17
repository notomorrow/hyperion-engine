/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_RECT_H
#define HYPERION_RECT_H

#include <math/Vector2.hpp>
#include <math/Vector4.hpp>

namespace hyperion {

struct Rect {
    union {
        struct {  // NOLINT(clang-diagnostic-nested-anon-types)
            float left,
                  right,
                  top,
                  bottom;
        };
    };

    Vector4 ToVector4() const
    {
        return { left, right, top, bottom };
    }
};

} // namespace hyperion

#endif
