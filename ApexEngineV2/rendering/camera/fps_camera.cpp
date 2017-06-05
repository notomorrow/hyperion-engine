#include "fps_camera.h"

namespace apex {
FpsCamera::FpsCamera(InputManager *inputmgr, RenderWindow *window, float fov, float near_clip, float far_clip)
    : PerspectiveCamera(fov, 512, 512, near_clip, far_clip), inputmgr(inputmgr), window(window)
{
    mag_x = 0.0;
    mag_y = 0.0;
    old_mag_x = 0.0;
    old_mag_y = 0.0;
    mouse_x = 0.0;
    mouse_y = 0.0;

    dir_cross_y = direction;
    dir_cross_y.Cross(up);

    is_mouse_captured = false;

    inputmgr->RegisterKeyEvent(KeyboardKey::KEY_LEFT_ALT, InputEvent([&]() {
        CenterMouse();
        is_mouse_captured = !is_mouse_captured;
    }));
}

void FpsCamera::SetTranslation(const Vector3 &vec)
{
    translation = vec;
    next_translation = vec;
}

void FpsCamera::UpdateLogic(double dt)
{
    width = window->width;
    height = window->height;

    if (is_mouse_captured) {
        mouse_x = inputmgr->GetMouseX();
        mouse_y = inputmgr->GetMouseY();
        CenterMouse();
        HandleMouseInput(dt, width / 2, height / 2);
    }
    HandleKeyboardInput(dt);
}

void FpsCamera::CenterMouse()
{
    inputmgr->SetMousePosition(width / 2, height / 2);
}

void FpsCamera::HandleMouseInput(double dt, int half_width, int half_height)
{
    const double sensitivity = 0.1;
    const double smoothing = 15.0;

    mag_x = mouse_x - half_width;
    mag_y = mouse_y - half_height;

    mag_x = MathUtil::Lerp(
        old_mag_x,
        mag_x,
        MathUtil::Clamp(smoothing * dt, 0.0, 1.0)
    );

    mag_y = MathUtil::Lerp(
        old_mag_y,
        mag_y,
        MathUtil::Clamp(smoothing * dt, 0.0, 1.0)
    );

    old_mag_x = mag_x;
    old_mag_y = mag_y;

    dir_cross_y = direction;
    dir_cross_y.Cross(up);

    Rotate(up, MathUtil::DegToRad(mag_x * sensitivity));
    Rotate(dir_cross_y, MathUtil::DegToRad(mag_y * sensitivity));

    if (direction.y > 0.97f || direction.y < -0.97f) {
        mag_y *= -1.0f;
        Rotate(dir_cross_y, MathUtil::DegToRad(mag_y * sensitivity));
    }
}

void FpsCamera::HandleKeyboardInput(double dt)
{
    const double speed = dt * 3.0;

    if (inputmgr->IsKeyDown(KEY_W)) {
        next_translation += direction * speed;
    } else if (inputmgr->IsKeyDown(KEY_S)) {
        next_translation -= direction * speed;
    }
    if (inputmgr->IsKeyDown(KEY_A)) {
        next_translation -= dir_cross_y * speed;
    } else if (inputmgr->IsKeyDown(KEY_D)) {
        next_translation += dir_cross_y * speed;
    }
    translation.Lerp(next_translation, MathUtil::Clamp(3.0 * dt, 0.0, 1.0));
}
}