#ifndef HYPERION_V2_SPATIAL_H
#define HYPERION_V2_SPATIAL_H

#include "material.h"

#include <rendering/backend/renderer_structs.h>

#include <math/matrix4.h>
#include <math/transform.h>

/* TMP */
#include "../../mesh.h"

namespace hyperion::v2 {

using renderer::MeshInputAttributeSet;

struct Spatial {
    uint32_t id; /* TODO: make spatial be an engine component so it gets an id by default */
    std::shared_ptr<Mesh> mesh; /* TMP */
    MeshInputAttributeSet attributes;
    Transform transform;
    Material::ID material_id;
};

} // namespace hyperion::v2

#endif