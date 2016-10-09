#ifndef SPHERE_H
#define SPHERE_H

#include "../../math/vector3.h"

namespace apex {

struct Sphere {
    Vector3 m_center;
    double m_radius;
};

} // namespace apex

#endif