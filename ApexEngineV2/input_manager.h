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
    InputEvent(std::function<void()> handler, bool is_up_evt = false);
    InputEvent(const InputEvent &other);

    inline bool IsEmpty() const { return is_empty; }
    inline bool IsUpEvent() const { return is_up_evt; }
    inline void Trigger() { handler(); }

private:
    bool is_up_evt, is_empty;
    std::function<void()> handler;
};

class InputManager {
public:
    InputManager();
    ~InputManager();

    inline double GetMouseX() const { return mouse_x; }
    inline double GetMouseY() const { return mouse_y; }
    inline void SetMousePosition(double x, double y) { CoreEngine::GetInstance()->SetMousePosition(x, y); }

    void KeyDown(int key);
    void KeyUp(int key);
    void MouseButtonDown(int btn);
    void MouseButtonUp(int btn);
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
};
} // namespace apex

#endif