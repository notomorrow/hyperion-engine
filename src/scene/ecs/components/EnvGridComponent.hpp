/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_ENV_GRID_COMPONENT_HPP
#define HYPERION_ECS_ENV_GRID_COMPONENT_HPP

#include <core/Handle.hpp>
#include <core/Name.hpp>
#include <core/lib/RefCountedPtr.hpp>
#include <scene/camera/Camera.hpp>
#include <math/Matrix4.hpp>
#include <math/Extent.hpp>
#include <math/Vector3.hpp>
#include <rendering/EnvGrid.hpp>
#include <HashCode.hpp>

namespace hyperion {

struct EnvGridComponent
{
    EnvGridType env_grid_type = ENV_GRID_TYPE_SH;
    Vec3u       grid_size = { 16, 4, 16 };

    RC<EnvGrid> render_component;

    HashCode    transform_hash_code;
};

} // namespace hyperion

#endif