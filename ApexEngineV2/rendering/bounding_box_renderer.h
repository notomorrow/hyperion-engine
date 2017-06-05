#ifndef BOUNDING_BOX_RENDERER_H
#define BOUNDING_BOX_RENDERER_H

#include "renderable.h"
#include "../math/bounding_box.h"
#include "mesh.h"

#include <memory>

namespace apex {

class BoundingBoxRenderer : public Renderable {
public:
    BoundingBoxRenderer(const BoundingBox *bounding_box);
    virtual ~BoundingBoxRenderer();

    virtual void Render() override;

private:
    const BoundingBox *m_bounding_box;
    Mesh *m_mesh;
};

} // namespace apex

#endif