#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#define NUM_KEYBOARD_KEYS 350
#define NUM_MOUSE_BUTTONS 3

#include "core_engine.h"

#include <functional>

namespace apex {

enum MouseButton {
    MOUSE_BTN_UNKNOWN = -1,
    MOUSE_BTN_LEFT,
    MOUSE_BTN_RIGHT,
    MOUSE_BTN_MIDDLE
};

enum KeyboardKey {
    KEY_UNKNOWN = -1,

    KEY_A = 'A',
    KEY_B,
    KEY_C,
    KEY_D,
    KEY_E,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_I,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_M,
    KEY_N,
    KEY_O,
    KEY_P,
    KEY_Q,
    KEY_R,
    KEY_S,
    KEY_T,
    KEY_U,
    KEY_V,
    KEY_W,
    KEY_X,
    KEY_Y,
    KEY_Z,

    KEY_0 = '0',
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,

    KEY_F1 = 290,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_F11,
    KEY_F12,

    KEY_LEFT_SHIFT = 340,
    KEY_LEFT_CTRL = 341,
    KEY_LEFT_ALT = 342,
    KEY_RIGHT_SHIFT = 344,
    KEY_RIGHT_CTRL = 345,
    KEY_RIGHT_ALT = 346,

    KEY_SPACE = 32,
    KEY_PERIOD = 46,
    KEY_RETURN = 257,
    KEY_TAB = 258,
    KEY_BACKSPACE = 259,
    KEY_CAPSLOCK = 280,

    KEY_ARROW_RIGHT = 262,
    KEY_ARROW_LEFT,
    KEY_ARROW_DOWN,
    KEY_ARROW_UP,
};

class InputEvent {
public:
    InputEvent();
    InputEvent(std::function<void(bool)> handler);
    InputEvent(const InputEvent &other);

    inline bool IsEmpty() const { return m_is_empty; }
    inline void Trigger(bool pressed)
    {
        if (m_is_empty) {
            return;
        }

        m_handler(pressed);
    }

    inline void SetHandler(const std::function<void(bool)> &handler) { m_handler = handler; }

private:
    bool m_is_empty;
    std::function<void(bool)> m_handler;
};

class InputManager {
public:
    InputManager();
    ~InputManager();

    inline double GetMouseX() const { return mouse_x; }
    inline double GetMouseY() const { return mouse_y; }
    inline void SetMousePosition(double x, double y) { CoreEngine::GetInstance()->SetMousePosition(x, y); }

    inline void KeyDown(int key) { SetKey(key, true); }
    inline void KeyUp(int key) { SetKey(key, false); }

    inline void MouseButtonDown(int btn) { SetMouseButton(btn, true); }
    inline void MouseButtonUp(int btn) { SetMouseButton(btn, false); }
    inline void MouseMove(double x, double y) { mouse_x = x; mouse_y = y; }

    bool IsKeyDown(int key) const;
    inline bool IsKeyUp(int key) const { return !IsKeyDown(key); }
    bool IsButtonDown(int btn) const;
    inline bool IsButtonUp(int btn) const { return !IsButtonDown(btn); }

    bool RegisterKeyEvent(int key, const InputEvent &evt);
    bool RegisterClickEvent(int btn, const InputEvent &evt);

private:
    bool key_states[NUM_KEYBOARD_KEYS];
    bool mouse_states[NUM_MOUSE_BUTTONS];
    InputEvent *key_events;
    InputEvent *mouse_events;
    double mouse_x, mouse_y;

    void SetKey(int key, bool pressed);
    void SetMouseButton(int btn, bool pressed);
};

} // namespace apex

#endif
