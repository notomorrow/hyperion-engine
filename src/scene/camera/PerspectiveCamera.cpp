/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/camera/PerspectiveCamera.hpp>

namespace hyperion::v2 {
PerspectiveCameraController::PerspectiveCameraController()
    : CameraController(CameraType::PERSPECTIVE)
{
}

void PerspectiveCameraController::OnAdded(Camera *camera)
{
    m_camera = camera;
}

void PerspectiveCameraController::UpdateLogic(double dt)
{
}

void PerspectiveCameraController::UpdateViewMatrix()
{
    m_camera->m_view_mat = Matrix4::LookAt(
        m_camera->m_translation,
        m_camera->GetTarget(),
        m_camera->m_up
    );
}

void PerspectiveCameraController::UpdateProjectionMatrix()
{
    m_camera->SetToPerspectiveProjection(
        m_camera->m_fov,
        m_camera->m_near,
        m_camera->m_far
    );
}
} // namespace hyperion::v2
