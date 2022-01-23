#include "renderable.h"

namespace hyperion {

Renderable::Renderable(RenderBucket bucket)
    : m_bucket(bucket)
{
}

bool Renderable::IntersectRay(const Ray &ray, const Transform &transform, RaytestHit &out) const
{
    return (m_aabb * transform).IntersectRay(ray, out);
}

bool Renderable::IntersectRay(const Ray &ray, const Transform &transform, RaytestHitList_t &out) const
{
    RaytestHit intersection;

    if (IntersectRay(ray, transform, intersection)) {
        out.push_back(intersection);

        return true;
    }

    return false;
}

} // namespace hyperion
