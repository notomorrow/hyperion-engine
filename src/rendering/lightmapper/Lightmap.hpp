#ifndef HYPERION_V2_LIGHTMAP_HPP
#define HYPERION_V2_LIGHTMAP_HPP

#include <scene/Entity.hpp>

#include <rendering/Mesh.hpp>

#include <math/Matrix4.hpp>

namespace hyperion::v2 {

struct LightmapEntity
{
    ID<Entity>      entity_id;
    Handle<Mesh>    mesh;
    Matrix4         transform;
};

} // namespace hyperion::v2

#endif