/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_PERSPECTIVE_CAMERA_HPP
#define HYPERION_PERSPECTIVE_CAMERA_HPP

#include <scene/camera/Camera.hpp>

namespace hyperion {
class HYP_API PerspectiveCameraController : public CameraController
{
public:
    PerspectiveCameraController();
    virtual ~PerspectiveCameraController() override = default;

    virtual void OnAdded(Camera *camera) override;

    virtual void UpdateLogic(double dt) override;
    virtual void UpdateViewMatrix() override;
    virtual void UpdateProjectionMatrix() override;
};
} // namespace hyperion

#endif
