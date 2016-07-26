#ifndef FPS_CAMERA_H
#define FPS_CAMERA_H

#include "../../input_manager.h"
#include "../../render_window.h"
#include "perspective_camera.h"

namespace apex {
class FpsCamera : public PerspectiveCamera {
public:
    FpsCamera(InputManager *inputmgr, RenderWindow *window, float fov, float n, float f);

    void UpdateLogic(double dt);

private:
    InputManager *inputmgr;
    RenderWindow *window;

    Vector3 dir_cross_y;
    Vector3 next_translation;

    double mouse_x, mouse_y;
    double mag_x, mag_y, old_mag_x, old_mag_y;
    bool is_mouse_captured;

    void CenterMouse();

    void HandleMouseInput(double dt, int half_width, int half_height);
    void HandleKeyboardInput(double dt);
};
}

#endif