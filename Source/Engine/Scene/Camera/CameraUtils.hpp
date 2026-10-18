/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Math/Transform.hpp>
#include <Core/Containers/SortedArray.hpp>

namespace Hyperion {

class Camera;

namespace CameraUtils {

static inline float ClampPitchDelta(const Camera& camera, float deltaRadians, float maxPitchSine)
{
    const float maxPitch = MathUtil::Arcsin(maxPitchSine);
    
    const float yClamp = MathUtil::Clamp(camera.GetDirection().y, -1.0f, 1.0f);

    const float currentPitch = MathUtil::Arcsin(yClamp);
    const float clampedPitch = MathUtil::Clamp(currentPitch - deltaRadians, -maxPitch, maxPitch);

    return currentPitch - clampedPitch;
}

static inline void ApplyYawPitch(
    Camera& camera,
    float yawRadians,
    float pitchRadians,
    float maxPitchSin)
{
    const float pitchDelta = ClampPitchDelta(camera, pitchRadians, maxPitchSin);

    Vec3f direction = Quat4f::AxisAngles(Vec3f::UnitY(), yawRadians)
        .RotateVector(camera.GetDirection());

    const Vec3f side = Vec3f::UnitY().Cross(direction).Normalized();
    direction = Quat4f::AxisAngles(side, pitchDelta).RotateVector(direction);

    camera.SetDirection(direction.Normalized());
}

} // namespace CameraUtils

} // namespace Hyperion