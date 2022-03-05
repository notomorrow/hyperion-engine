#ifndef FPS_CAMERA_H
#define FPS_CAMERA_H

#include "../../input_manager.h"
#include "../../system/sdl_system.h"
#include "perspective_camera.h"

namespace hyperion {
class FpsCamera : public PerspectiveCamera {
public:
    FpsCamera(InputManager *inputmgr, SystemWindow *window, int width, int height, float fov, float near, float far);
    virtual ~FpsCamera() = default;

    virtual void SetTranslation(const Vector3 &vec) override;

    void UpdateLogic(double dt);

private:
    InputManager *m_inputmgr;
    SystemWindow *m_window;

    Vector3 m_dir_cross_y;
    Vector3 m_next_translation;

    int m_mouse_x, m_mouse_y, m_old_mouse_x, m_old_mouse_y;
    double m_mag_x, m_mag_y, m_old_mag_x, m_old_mag_y;
    bool m_is_mouse_captured;

    void CenterMouse();

    void HandleMouseInput(double dt, int half_width, int half_height);
    void HandleKeyboardInput(double dt);
};
} // namespace hyperion

#endif
