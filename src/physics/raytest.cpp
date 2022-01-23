#include "raytest.h"

namespace hyperion {

void RayTestSphere(const Ray &ray, const Sphere &sphere)
{
    /*Vector3 vpc = sphere.m_center - ray.position;
    Vector3 abs_vpc = Vector3::Abs(vpc);

    Vector3 intersection;

    if (vpc.Dot(ray.direction) < 0.0f) {
        if (abs_vpc.Length() > sphere.m_radius) {
            // no intersection
        } else if (abs_vpc.Length() == sphere.m_radius) {
            intersection = ray.position;
        } else {

        }
    }*/
}

} // namespace hyperion
