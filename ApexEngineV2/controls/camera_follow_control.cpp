#include "camera_follow_control.h"

namespace apex {
CameraFollowControl::CameraFollowControl(Camera *camera, const Vector3 &offset)
    : EntityControl(60.0),
      m_camera(camera),
      m_offset(offset)
{
}

CameraFollowControl::~CameraFollowControl()
{
}

void CameraFollowControl::OnAdded()
{
}

void CameraFollowControl::OnRemoved()
{
}

void CameraFollowControl::OnUpdate(double dt)
{
    parent->SetGlobalTranslation(m_camera->GetTranslation() + m_offset);
}
} // namespace apex
