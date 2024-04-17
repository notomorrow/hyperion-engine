/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ENTITY_DRAW_DATA_HPP
#define HYPERION_ENTITY_DRAW_DATA_HPP

#include <core/ID.hpp>
#include <core/utilities/UserData.hpp>

#include <math/Transform.hpp>
#include <math/BoundingBox.hpp>
#include <math/Matrix4.hpp>

namespace hyperion {

class Entity;
class Mesh;
class Material;
class Skeleton;

struct EntityDrawData
{
    ID<Entity>                              entity_id;
    ID<Mesh>                                mesh_id;
    ID<Material>                            material_id;
    ID<Skeleton>                            skeleton_id;
    Matrix4                                 model_matrix;
    Matrix4                                 previous_model_matrix;
    BoundingBox                             aabb;
    uint32                                  bucket;
    UserData<sizeof(Vec4u), alignof(Vec4u)> user_data;
};

} // namespace hyperion

#endif