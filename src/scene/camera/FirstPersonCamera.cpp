/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/camera/FirstPersonCamera.hpp>

#include <util/profiling/ProfileScope.hpp>

namespace hyperion {

static const float mouse_sensitivity = 1.0f;
static const float mouse_blending = 0.35f;
static const float movement_speed = 5.0f;
static const float movement_speed_2 = movement_speed * 2.0f;
static const float movement_blending = 0.01f;

#pragma region FirstPersonCameraInputHandler

class FirstPersonCameraInputHandler : public InputHandler
{
public:
    FirstPersonCameraInputHandler(CameraController *controller)
        : m_controller(dynamic_cast<FirstPersonCameraController *>(controller))
    {
        AssertThrowMsg(m_controller != nullptr, "Null camera controller or not of type FirstPersonCameraInputHandler");
    }

    virtual bool OnKeyDown(const KeyboardEvent &event) override
    {
        HYP_SCOPE;

        Camera *camera = m_controller->GetCamera();

        if (event.key_code == KeyCode::KEY_W || event.key_code == KeyCode::KEY_S || event.key_code == KeyCode::KEY_A || event.key_code == KeyCode::KEY_D) {
            CameraController *camera_controller = camera->GetCameraController();

            Vec3f translation = camera->GetTranslation();

            const Vec3f direction = camera->GetDirection();
            const Vec3f dir_cross_y = direction.Cross(camera->GetUpVector());

            if (event.key_code == KeyCode::KEY_W) {
                translation += direction * 0.01f;
            }
            if (event.key_code == KeyCode::KEY_S) {
                translation -= direction * 0.01f;
            }
            if (event.key_code == KeyCode::KEY_A) {
                translation += dir_cross_y * 0.01f;
            }
            if (event.key_code == KeyCode::KEY_D) {
                translation -= dir_cross_y * 0.01f;
            }

            camera_controller->SetNextTranslation(translation);

            return true;
        }

        return false;
    }

    virtual bool OnKeyUp(const KeyboardEvent &event) override
    {
        HYP_SCOPE;

        return false;
    }

    virtual bool OnMouseDown(const MouseEvent &event) override
    {
        HYP_SCOPE;

        m_controller->SetMode(FirstPersonCameraControllerMode::MOUSE_LOCKED);

        return true;
    }

    virtual bool OnMouseUp(const MouseEvent &event) override
    {
        HYP_SCOPE;

        m_controller->SetMode(FirstPersonCameraControllerMode::MOUSE_FREE);

        return true;
    }

    virtual bool OnMouseMove(const MouseEvent &event) override
    {
        HYP_SCOPE;

        return false;
    }

    virtual bool OnMouseDrag(const MouseEvent &event) override
    {
        HYP_SCOPE;

        Camera *camera = m_controller->GetCamera();
        
        if (!camera) {
            return false;
        }

        const Vec2f delta = (event.position - event.previous_position) * 150.0f;

        const Vec3f dir_cross_y = camera->GetDirection().Cross(camera->GetUpVector());

        camera->Rotate(camera->GetUpVector(), MathUtil::DegToRad(delta.x));
        camera->Rotate(dir_cross_y, MathUtil::DegToRad(delta.y));

        if (camera->GetDirection().y > 0.98f || camera->GetDirection().y < -0.98f) {
            camera->Rotate(dir_cross_y, MathUtil::DegToRad(-delta.y));
        }

        return true;
    }

    virtual bool OnClick(const MouseEvent &event) override
    {
        return false;
    }

private:
    FirstPersonCameraController *m_controller;
};

#pragma endregion FirstPersonCameraInputHandler

#pragma region FirstPersonCameraController

FirstPersonCameraController::FirstPersonCameraController(FirstPersonCameraControllerMode mode)
    : PerspectiveCameraController(),
      m_mode(mode),
      m_mouse_x(0.0f),
      m_mouse_y(0.0f),
      m_prev_mouse_x(0.0f),
      m_prev_mouse_y(0.0f)
{
    m_input_handler = MakeUnique<FirstPersonCameraInputHandler>(this);
}

void FirstPersonCameraController::OnActivated()
{
    HYP_SCOPE;
    
    PerspectiveCameraController::OnActivated();
}

void FirstPersonCameraController::OnDeactivated()
{
    HYP_SCOPE;
    
    PerspectiveCameraController::OnDeactivated();
}

void FirstPersonCameraController::SetMode(FirstPersonCameraControllerMode mode)
{
    HYP_SCOPE;
    
    if (m_mode == mode) {
        return;
    }

    switch (mode) {
    case FirstPersonCameraControllerMode::MOUSE_FREE:
        CameraController::SetIsMouseLockRequested(false);

        break;
    case FirstPersonCameraControllerMode::MOUSE_LOCKED:
        CameraController::SetIsMouseLockRequested(true);

        break;
    }

    m_mode = mode;
}

void FirstPersonCameraController::UpdateLogic(double dt)
{
    HYP_SCOPE;
    
    // m_desired_mag = Vec2f {
    //     m_mouse_x - m_prev_mouse_x,
    //     m_mouse_y - m_prev_mouse_y
    // };
    
    // m_mag.Lerp(m_desired_mag, MathUtil::Min(1.0f, 1.0f - mouse_blending));

    // m_dir_cross_y = m_camera->GetDirection().Cross(m_camera->GetUpVector());

    // // m_camera->Rotate(m_camera->GetUpVector(), MathUtil::DegToRad(m_mag.x * mouse_sensitivity));
    // // m_camera->Rotate(m_dir_cross_y, MathUtil::DegToRad(m_mag.y * mouse_sensitivity));

    // // if (m_camera->GetDirection().y > 0.98f || m_camera->GetDirection().y < -0.98f) {
    // //     m_camera->Rotate(m_dir_cross_y, MathUtil::DegToRad(-m_mag.y * mouse_sensitivity));
    // // }

    // m_prev_mouse_x = m_mouse_x;
    // m_prev_mouse_y = m_mouse_y;

    // m_camera->m_next_translation += m_move_deltas * movement_speed * float(dt);

    // if (movement_blending > 0.0f) {
    //     m_move_deltas.Lerp(
    //         Vector3::Zero(),
    //         MathUtil::Clamp(
    //             (1.0f / movement_blending) * float(dt),
    //             0.0f,
    //             1.0f
    //         )
    //     );
    // } else {
    //     m_move_deltas.Lerp(
    //         Vector3::Zero(),
    //         MathUtil::Clamp(
    //             1.0f * float(dt),
    //             0.0f,
    //             1.0f
    //         )
    //     );
    // }

    // switch (m_mode) {
    // case FirstPersonCameraControllerMode::MOUSE_LOCKED:
    //     m_prev_mouse_x = m_camera->GetWidth() / 2;
    //     m_prev_mouse_y = m_camera->GetHeight() / 2;

    //     break;
    // }
}

void FirstPersonCameraController::RespondToCommand(const CameraCommand &command, GameCounter::TickUnit dt)
{
    HYP_SCOPE;
    
    switch (command.command) {
    case CameraCommand::CAMERA_COMMAND_MAG:
    {
        m_mouse_x = command.mag_data.mouse_x;
        m_mouse_y = command.mag_data.mouse_y;
    
        break;
    }
    case CameraCommand::CAMERA_COMMAND_MOVEMENT:
    {
        const float speed = movement_speed;// * static_cast<float>(dt);

        switch (command.movement_data.movement_type) {
        case CameraCommand::CAMERA_MOVEMENT_FORWARD:
            m_move_deltas += m_camera->m_direction;// * speed;

            break;
        case CameraCommand::CAMERA_MOVEMENT_BACKWARD:
            m_move_deltas -= m_camera->m_direction;// * speed;

            break;
        case CameraCommand::CAMERA_MOVEMENT_LEFT:
            m_move_deltas -= m_dir_cross_y;// * speed;

            break;
        case CameraCommand::CAMERA_MOVEMENT_RIGHT:
            m_move_deltas += m_dir_cross_y;// * speed;

            break;
        default:
            break;
        }
    
        break;

    }

    default:
        break;
    }
}

#pragma endregion FirstPersonCameraController

} // namespace hyperion
