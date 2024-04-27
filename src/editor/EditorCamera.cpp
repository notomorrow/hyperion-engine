/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorCamera.hpp>

namespace hyperion {

EditorCameraController::EditorCameraController()
    : FirstPersonCameraController(),
      m_mode(EC_MODE_INACTIVE)
{
}

void EditorCameraController::SetMode(EditorCameraControllerMode mode)
{
    if (m_mode == mode) {
        return;
    }

    m_mode = mode;

    switch (m_mode) {
    case EC_MODE_INACTIVE:
        // FirstPersonCameraController::SetMode(FPC_MODE_MOUSE_FREE);
        break;
    case EC_MODE_FOCUSED:
        // FirstPersonCameraController::SetMode(FPC_MODE_MOUSE_LOCKED);
        break;
    }
}

void EditorCameraController::UpdateLogic(double dt)
{
    FirstPersonCameraController::UpdateLogic(dt);
}

void EditorCameraController::RespondToCommand(const CameraCommand &command, GameCounter::TickUnit dt)
{
    switch (command.command) {
    case CameraCommand::CAMERA_COMMAND_MAG:
    case CameraCommand::CAMERA_COMMAND_MOVEMENT: // fallthrough
        if (m_mode == EC_MODE_INACTIVE) {
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

} // namespace hyperion
