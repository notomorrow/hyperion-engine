#include "spatial.h"

namespace hyperion {

Spatial::Spatial(Bucket bucket)
    : m_bucket(bucket),
      m_renderable(nullptr)
{
}

Spatial::Spatial(Bucket bucket,
    const std::shared_ptr<Renderable> &renderable,
    const Material &material,
    const BoundingBox &aabb,
    const Transform &transform)
    : m_bucket(bucket),
      m_renderable(renderable),
      m_material(material),
      m_aabb(aabb),
      m_transform(transform)
{
}

Spatial::Spatial(const Spatial &other)
    : m_bucket(other.m_bucket),
      m_renderable(other.m_renderable),
      m_material(other.m_material),
      m_aabb(other.m_aabb),
      m_transform(other.m_transform)
{
}

Spatial &Spatial::operator=(const Spatial &other)
{
    m_bucket = other.m_bucket;
    m_renderable = other.m_renderable;
    m_material = other.m_material;
    m_aabb = other.m_aabb;
    m_transform = other.m_transform;

    return *this;
}
} // namespace hyperion
