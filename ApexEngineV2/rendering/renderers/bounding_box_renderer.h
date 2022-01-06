#ifndef BOUNDING_BOX_RENDERER_H
#define BOUNDING_BOX_RENDERER_H

#include <cstddef>
#include <vector>

#include "../renderable.h"
#include "../../math/bounding_box.h"
#include "../mesh.h"

namespace apex {

class BoundingBoxRenderer : public Renderable {
    static const std::vector<MeshIndex> indices;
public:
    BoundingBoxRenderer();
    BoundingBoxRenderer(const BoundingBox &) = delete;
    virtual ~BoundingBoxRenderer();

    virtual void Render() override;

    inline void SetAABB(const BoundingBox &aabb) { m_aabb = aabb; } 

private:
    void UpdateVertices();

    Mesh *m_mesh;
    std::vector<Vertex> m_vertices;
};

} // namespace apex

#endif
