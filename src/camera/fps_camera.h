#ifndef FPS_CAMERA_H
#define FPS_CAMERA_H

#include <math/vector2.h>
#include <input_manager.h>
#include <system/sdl_system.h>
#include "perspective_camera.h"

namespace hyperion {
class FpsCamera : public PerspectiveCamera {
public:
    static constexpr float mouse_sensitivity = 0.08f;
    static constexpr float movement_speed = 10.0f;
    static constexpr float movement_speed_2 = movement_speed * 2.0f;
    static constexpr float movement_blending = 0.15f;

    FpsCamera(InputManager *inputmgr, SystemWindow *window, int width, int height, float fov, float _near, float _far);
    virtual ~FpsCamera() = default;

    virtual void SetTranslation(const Vector3 &vec) override;
    virtual void UpdateLogic(double dt) override;

private:
    InputManager *m_inputmgr;
    SystemWindow *m_window;

    Vector3 m_dir_cross_y;
    Vector3 m_next_translation;

    int m_mouse_x, m_mouse_y;
    Vector2 m_mag;
    bool m_is_mouse_captured;

    void CenterMouse();

    void HandleMouseInput(double dt, int half_width, int half_height);
    void HandleKeyboardInput(double dt);
};
} // namespace hyperion

#endif
