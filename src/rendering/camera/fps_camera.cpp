#include "fps_camera.h"

namespace hyperion {
FpsCamera::FpsCamera(InputManager *inputmgr, SystemWindow *window, int width, int height, float fov, float _near, float _far)
    : PerspectiveCamera(fov, width, height, _near, _far),
      m_inputmgr(inputmgr),
      m_window(window),
      m_mouse_x(0.0),
      m_mouse_y(0.0),
      m_is_mouse_captured(false)
{
    m_dir_cross_y = Vector3(m_direction).Cross(m_up);

    m_inputmgr->RegisterKeyEvent(SystemKey::KEY_LEFT_ALT, InputEvent([&](SystemWindow *window, bool pressed) {
        if (pressed) {
            CenterMouse();
            m_is_mouse_captured = !m_is_mouse_captured;
            DebugLog(LogType::Info, "Mouse state switched [%d]  %s\n", m_is_mouse_captured, typeid(*this).name());

            window->LockMouse(m_is_mouse_captured);
        }
    }));
}

void FpsCamera::SetTranslation(const Vector3 &vec)
{
    Camera::SetTranslation(vec);
    m_next_translation = m_translation;
}

void FpsCamera::UpdateLogic(double dt)
{
    m_window->GetSize(&m_width, &m_height);

    if (m_is_mouse_captured) {
        m_inputmgr->GetMousePosition(&m_mouse_x, &m_mouse_y);

        HandleMouseInput(dt, m_width / 2, m_height / 2);

        CenterMouse();
    }

    HandleKeyboardInput(dt);
}

void FpsCamera::CenterMouse()
{
    m_window->GetSize(&m_width, &m_height);
    m_inputmgr->SetMousePosition(m_width / 2, m_height / 2);
}

void FpsCamera::HandleMouseInput(double dt, int half_width, int half_height)
{
    m_mag = {
        static_cast<float>(m_mouse_x - half_width),
        static_cast<float>(m_mouse_y - half_height)
    };

    m_dir_cross_y = Vector3(m_direction).Cross(m_up);

    Rotate(m_up, MathUtil::DegToRad(m_mag.x * mouse_sensitivity));
    Rotate(m_dir_cross_y, MathUtil::DegToRad(m_mag.y * mouse_sensitivity));

    if (m_direction.y > 0.97f || m_direction.y < -0.97f) {
        m_mag.y *= -1.0f;

        Rotate(m_dir_cross_y, MathUtil::DegToRad(m_mag.y * mouse_sensitivity));
    }
}

void FpsCamera::HandleKeyboardInput(double dt)
{
    float speed = movement_speed * static_cast<float>(dt);

    if (m_inputmgr->IsKeyDown(KEY_LEFT_SHIFT) || m_inputmgr->IsKeyDown(KEY_RIGHT_SHIFT)) {
        speed = movement_speed_2 * static_cast<float>(dt);
    }

    if (m_inputmgr->IsKeyDown(KEY_W)) {
        m_next_translation += m_direction * speed;
    } else if (m_inputmgr->IsKeyDown(KEY_S)) {
        m_next_translation -= m_direction * speed;
    }

    if (m_inputmgr->IsKeyDown(KEY_A)) {
        m_next_translation -= m_dir_cross_y * speed;
    } else if (m_inputmgr->IsKeyDown(KEY_D)) {
        m_next_translation += m_dir_cross_y * speed;
    }

    constexpr float blending = (movement_blending > 0.0f)
        ? 1.0f / movement_blending
        : 1.0f;

    m_translation.Lerp(
        m_next_translation,
        MathUtil::Clamp(
            blending * static_cast<float>(dt),
            0.0f,
            1.0f
        )
    );
}
} // namespace hyperion
