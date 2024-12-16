/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FIRST_PERSON_CAMERA_HPP
#define HYPERION_FIRST_PERSON_CAMERA_HPP

#include <scene/camera/PerspectiveCamera.hpp>

#include <math/Vector2.hpp>

namespace hyperion {

HYP_ENUM()
enum class FirstPersonCameraControllerMode : uint32
{
    MOUSE_LOCKED    = 0,
    MOUSE_FREE      = 1
};

HYP_CLASS()
class HYP_API FirstPersonCameraController : public PerspectiveCameraController
{
    HYP_OBJECT_BODY(FirstPersonCameraController);

public:
    FirstPersonCameraController(FirstPersonCameraControllerMode mode = FirstPersonCameraControllerMode::MOUSE_FREE);
    virtual ~FirstPersonCameraController() = default;
    
    HYP_METHOD(Property="Mode")
    FirstPersonCameraControllerMode GetMode() const
        { return m_mode; }

    HYP_METHOD(Property="Mode")
    void SetMode(FirstPersonCameraControllerMode mode);

    HYP_METHOD()
    virtual bool IsMouseLockAllowed() const override
        { return true; }

    virtual void UpdateLogic(double dt) override;

protected:
    virtual void OnActivated() override;
    virtual void OnDeactivated() override;

    virtual void RespondToCommand(const CameraCommand &command, GameCounter::TickUnit dt) override;

    FirstPersonCameraControllerMode m_mode;

    Vec3f                           m_move_deltas;
    Vec3f                           m_dir_cross_y;

    float                           m_mouse_x;
    float                           m_mouse_y;
    float                           m_prev_mouse_x;
    float                           m_prev_mouse_y;
    
    Vec2f                           m_mag;
    Vec2f                           m_desired_mag;
    Vec2f                           m_prev_mag;
};

} // namespace hyperion

#endif
