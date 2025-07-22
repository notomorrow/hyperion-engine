#include <rendering/util/ShadowCameraHelper.hpp>

#include <scene/camera/Camera.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_API void ShadowCameraHelper::UpdateShadowCameraDirectional(
    const Handle<Camera>& camera,
    const Vec3f& center,
    const Vec3f& dir,
    float radius,
    BoundingBox& outAabb)
{
    AssertDebug(camera.IsValid());

    camera->SetTranslation(center + dir);
    camera->SetTarget(center);

    BoundingBox aabb { center - radius, center + radius };

    FixedArray<Vec3f, 8> corners = aabb.GetCorners();

    for (Vec3f& corner : corners)
    {
        corner = camera->GetViewMatrix() * corner;

        aabb.max = MathUtil::Max(aabb.max, corner);
        aabb.min = MathUtil::Min(aabb.min, corner);
    }

    aabb.max.z = radius;
    aabb.min.z = -radius;

    camera->SetToOrthographicProjection(aabb.min.x, aabb.max.x, aabb.min.y, aabb.max.y, aabb.min.z, aabb.max.z);
    
    HYP_LOG_TEMP("Updated shadow camera matrix to: {}", camera->GetViewProjectionMatrix());

    outAabb = aabb;
}

} // namespace hyperion
