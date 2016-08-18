#ifndef FRUSTUM_H
#define FRUSTUM_H

#include "matrix4.h"
#include "vector4.h"

#include <array>

namespace apex {
class Frustum {
public:
    Frustum(const Matrix4 &view_proj);

    Vector4 &GetPlane(size_t index);
    const Vector4 &GetPlane(size_t index) const;

private:
    std::array<Vector4, 6> planes;
};
}

#endif

