/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/Handle.hpp>

#include <core/math/Vector3.hpp>
#include <core/math/BoundingBox.hpp>

namespace hyperion {

class Camera;

class ShadowCameraHelper
{
public:
    static HYP_API void UpdateShadowCameraDirectional(
        const Handle<Camera>& camera,
        const Vec3f& center,
        const Vec3f& dir,
        float radius,
        BoundingBox& outAabb);
};

} // namespace hyperion
