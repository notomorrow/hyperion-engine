#ifndef RAYTEST_H
#define RAYTEST_H

#include "shapes/sphere.h"
#include "../math/bounding_box.h"
#include "../math/ray.h"

namespace hyperion {

void RayTestSphere(const Ray &ray, const Sphere &sphere);

} // namespace hyperion

#endif
