#ifndef BOUNDING_BOX_RENDERER_H
#define BOUNDING_BOX_RENDERER_H

#include <cstddef>
#include <vector>

#include "renderable.h"
#include "../math/bounding_box.h"
#include "mesh.h"

namespace apex {

class BoundingBoxRenderer : public Renderable {
    static const std::vector<uint32_t> indices;
public:
    BoundingBoxRenderer(const BoundingBox *bounding_box);
    BoundingBoxRenderer(const BoundingBox &) = delete;
    virtual ~BoundingBoxRenderer();

    virtual void Render() override;

private:
    void UpdateVertices();

    const BoundingBox *m_bounding_box;
    Mesh *m_mesh;

    std::vector<Vertex> m_vertices;
};

} // namespace apex

#endif
