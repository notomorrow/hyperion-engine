/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorCamera.hpp>

#include <input/InputManager.hpp>
#include <input/InputHandler.hpp>

#include <util/profiling/ProfileScope.hpp>

namespace hyperion {

#pragma region EditorCameraInputHandler

EditorCameraInputHandler::EditorCameraInputHandler(CameraController *controller)
    : m_controller(dynamic_cast<EditorCameraController *>(controller))
{
    AssertThrowMsg(m_controller != nullptr, "Null camera controller or not of type EditorCameraController");
}

bool EditorCameraInputHandler::OnKeyDown_Impl(const KeyboardEvent &evt)
{
    HYP_SCOPE;

    Camera *camera = m_controller->GetCamera();

    if (evt.key_code == KeyCode::KEY_W || evt.key_code == KeyCode::KEY_S || evt.key_code == KeyCode::KEY_A || evt.key_code == KeyCode::KEY_D) {
        CameraController *camera_controller = camera->GetCameraController();

        Vec3f translation = camera->GetTranslation();

        const Vec3f direction = camera->GetDirection();
        const Vec3f dir_cross_y = direction.Cross(camera->GetUpVector());

        if (evt.key_code == KeyCode::KEY_W) {
            translation += direction * 0.01f;
        }
        if (evt.key_code == KeyCode::KEY_S) {
            translation -= direction * 0.01f;
        }
        if (evt.key_code == KeyCode::KEY_A) {
            translation += dir_cross_y * 0.01f;
        }
        if (evt.key_code == KeyCode::KEY_D) {
            translation -= dir_cross_y * 0.01f;
        }

        camera_controller->SetNextTranslation(translation);

        return true;
    }

    return false;
}

bool EditorCameraInputHandler::OnKeyUp_Impl(const KeyboardEvent &evt)
{
    HYP_SCOPE;

    return false;
}

bool EditorCameraInputHandler::OnMouseDown_Impl(const MouseEvent &evt)
{
    HYP_SCOPE;

    m_controller->SetMode(EditorCameraControllerMode::MOUSE_LOCKED);

    return true;
}

bool EditorCameraInputHandler::OnMouseUp_Impl(const MouseEvent &evt)
{
    HYP_SCOPE;

    m_controller->SetMode(EditorCameraControllerMode::INACTIVE);

    return true;
}

bool EditorCameraInputHandler::OnMouseMove_Impl(const MouseEvent &evt)
{
    HYP_SCOPE;

    return false;
}

bool EditorCameraInputHandler::OnMouseDrag_Impl(const MouseEvent &evt)
{
    HYP_SCOPE;

    Camera *camera = m_controller->GetCamera();
    
    if (!camera) {
        return false;
    }

    const Vec2f delta = (evt.position - evt.previous_position) * 150.0f;

    const Vec3f dir_cross_y = camera->GetDirection().Cross(camera->GetUpVector());

    if (evt.mouse_buttons & MouseButtonState::LEFT) {
        const Vec2i delta_sign = Vec2i(MathUtil::Sign(delta));

        if (evt.mouse_buttons & MouseButtonState::RIGHT) {
            camera->SetNextTranslation(camera->GetTranslation() + dir_cross_y * 0.1f * float(-delta_sign.y));
        } else {
            camera->Rotate(camera->GetUpVector(), MathUtil::DegToRad(delta.x));

            camera->SetNextTranslation(camera->GetTranslation() + camera->GetDirection() * 0.1f * float(-delta_sign.y));
        }
    } else if (evt.mouse_buttons & MouseButtonState::RIGHT) {
        camera->Rotate(camera->GetUpVector(), MathUtil::DegToRad(delta.x));
        camera->Rotate(dir_cross_y, MathUtil::DegToRad(delta.y));

        if (camera->GetDirection().y > 0.98f || camera->GetDirection().y < -0.98f) {
            camera->Rotate(dir_cross_y, MathUtil::DegToRad(-delta.y));
        }
    }

    return true;
}

bool EditorCameraInputHandler::OnClick_Impl(const MouseEvent &evt)
{
    return false;
}

#pragma endregion EditorCameraInputHandler

#pragma region EditorCameraController

EditorCameraController::EditorCameraController()
    : FirstPersonCameraController(),
      m_mode(EditorCameraControllerMode::INACTIVE)
{
    m_input_handler = MakeRefCountedPtr<EditorCameraInputHandler>(this);
}

void EditorCameraController::OnActivated()
{
    HYP_SCOPE;

    FirstPersonCameraController::OnActivated();
}

void EditorCameraController::SetMode(EditorCameraControllerMode mode)
{
    HYP_SCOPE;

    if (m_mode == mode) {
        return;
    }

    switch (mode) {
    case EditorCameraControllerMode::INACTIVE:
    case EditorCameraControllerMode::FOCUSED: // fallthrough
        FirstPersonCameraController::SetMode(FirstPersonCameraControllerMode::MOUSE_FREE);
        
        break;
    case EditorCameraControllerMode::MOUSE_LOCKED:
        FirstPersonCameraController::SetMode(FirstPersonCameraControllerMode::MOUSE_LOCKED);

        break;
    }

    m_mode = mode;
}

void EditorCameraController::UpdateLogic(double dt)
{
    HYP_SCOPE;

    FirstPersonCameraController::UpdateLogic(dt);
}

void EditorCameraController::RespondToCommand(const CameraCommand &command, GameCounter::TickUnit dt)
{
    HYP_SCOPE;

    switch (command.command) {
    case CameraCommand::CAMERA_COMMAND_MAG:
    case CameraCommand::CAMERA_COMMAND_MOVEMENT: // fallthrough
        if (m_mode == EditorCameraControllerMode::INACTIVE) {
            // Don't handle command
            return;
        }
    
        // Let base class handle command
        break;

    default:
        break;
    }

    FirstPersonCameraController::RespondToCommand(command, dt);
}

#pragma endregion EditorCameraController

} // namespace hyperion
