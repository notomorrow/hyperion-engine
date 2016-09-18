#ifndef FRUSTUM_H
#define FRUSTUM_H

#include "matrix4.h"
#include "vector4.h"

#include <array>

namespace apex {
class Frustum {
public:
    Frustum(const Matrix4 &view_proj);

    inline Vector4 &GetPlane(size_t index) { return m_planes[index]; }
    inline const Vector4 &GetPlane(size_t index) const { return m_planes[index]; }

private:
    std::array<Vector4, 6> m_planes;
};
} // namespace apex

#endif

