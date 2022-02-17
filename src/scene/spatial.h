#ifndef SPATIAL_H
#define SPATIAL_H

#include "../rendering/renderable.h"
#include "../rendering/material.h"
#include "../math/bounding_box.h"
#include "../math/transform.h"
#include "../hash_code.h"

#include <memory>

namespace hyperion {

class Spatial {
    friend class Node;
public:
    enum Bucket {
        RB_SKY,
        RB_OPAQUE,
        RB_TRANSPARENT,
        RB_PARTICLE,
        RB_SCREEN,
        RB_DEBUG,
        RB_BUFFER,
        // ...
        RB_MAX
    };

    Spatial(Bucket bucket = RB_OPAQUE);
    Spatial(Bucket bucket,
        const std::shared_ptr<Renderable> &renderable,
        const Material &material,
        const BoundingBox &aabb,
        const Transform &transform);
    Spatial(const Spatial &other);
    ~Spatial() = default;

    Spatial &operator=(const Spatial &other);

    inline bool operator==(const Spatial &other) const
    {
        return m_bucket == other.m_bucket
            && m_renderable == other.m_renderable
            && m_material == other.m_material
            && m_aabb == other.m_aabb
            && m_transform == other.m_transform;
    }

    inline Bucket GetBucket() const { return m_bucket; }
    inline void SetBucket(Bucket bucket) { m_bucket = bucket; }
    inline std::shared_ptr<Renderable> &GetRenderable() { return m_renderable; }
    inline const std::shared_ptr<Renderable> &GetRenderable() const { return m_renderable; }
    inline Material &GetMaterial() { return m_material; }
    inline const Material &GetMaterial() const { return m_material; }
    inline BoundingBox &GetAABB() { return m_aabb; }
    inline const BoundingBox &GetAABB() const { return m_aabb; }
    inline Transform &GetTransform() { return m_transform; }
    inline const Transform &GetTransform() const { return m_transform; }

    inline HashCode GetHashCode() const
    {
        HashCode hc;

        //if (m_renderable != nullptr) {
        //  hc.Add(m_renderable->GetHashCode());
        //}
        hc.Add(int(m_bucket));
        hc.Add(intptr_t(m_renderable.get())); // for now
        hc.Add(m_material.GetHashCode());
        hc.Add(m_aabb.GetHashCode());
        hc.Add(m_transform.GetHashCode());

        return hc;
    }

private:
    Bucket m_bucket;
    std::shared_ptr<Renderable> m_renderable;
    Material m_material;
    BoundingBox m_aabb;
    Transform m_transform;
};

} // namespace hyperion

#endif