#include "follow_camera_controller.h"
#include <engine.h>

namespace hyperion::v2 {

FollowCameraController::FollowCameraController()
    : Controller("FollowCameraController")
{
}

void FollowCameraController::OnAdded()
{
}

void FollowCameraController::OnRemoved()
{
}

void FollowCameraController::OnUpdate(GameCounter::TickUnit)
{
    if (const auto *scene = GetOwner()->GetScene()) {
        if (const auto *camera = scene->GetCamera()) {
            GetOwner()->SetTranslation(camera->GetTranslation());
        }
    }
}

} // namespace hyperion::v2