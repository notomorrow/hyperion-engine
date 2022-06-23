#ifndef HYPERION_V2_FOLLOW_CAMERA_CONTROLLER_H
#define HYPERION_V2_FOLLOW_CAMERA_CONTROLLER_H

#include "../controller.h"

namespace hyperion::v2 {

class Engine;

class FollowCameraController : public Controller {
public:
    FollowCameraController();
    virtual ~FollowCameraController() override = default;
    
    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;
};

} // namespace hyperion::v2

#endif
