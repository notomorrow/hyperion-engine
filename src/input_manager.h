#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#define NUM_KEYBOARD_KEYS 350
#define NUM_MOUSE_BUTTONS 3

#include "core_engine.h"
#include "system/sdl_system.h"

#include <functional>

namespace hyperion {

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
    InputManager(SystemWindow *window);
    ~InputManager();

    void CheckEvent(SystemEvent *event);

    inline void GetMousePosition(int *x, int *y) { this->GetWindow()->GetMouseState(x, y); }
    inline void SetMousePosition(int x,  int y) { this->GetWindow()->SetMousePosition(x, y); }

    inline void KeyDown(int key) { DebugLog(LogType::Info, "Button %c(%d) pressed\n", key, key); SetKey(key, true); }
    inline void KeyUp(int key) { SetKey(key, false); }

    inline void MouseButtonDown(int btn) { SetMouseButton(btn, true); }
    inline void MouseButtonUp(int btn) { SetMouseButton(btn, false); }
    inline void MouseMove(double x, double y) { mouse_x = x; mouse_y = y; }

    bool IsKeyDown(int key) const;
    inline bool IsKeyUp(int key) const { return !IsKeyDown(key); }
    bool IsButtonDown(int btn) const;
    inline bool IsButtonUp(int btn) const { return !IsButtonDown(btn); }

    void SetWindow(SystemWindow *_window) { this->window = _window; };
    inline SystemWindow *GetWindow() { return this->window; };

    bool RegisterKeyEvent(int key, const InputEvent &evt);
    bool RegisterClickEvent(int btn, const InputEvent &evt);

private:
    bool key_states[NUM_KEYBOARD_KEYS];
    bool mouse_states[NUM_MOUSE_BUTTONS];
    InputEvent *key_events;
    InputEvent *mouse_events;
    double mouse_x, mouse_y;

    SystemWindow *window = nullptr;

    void SetKey(int key, bool pressed);
    void SetMouseButton(int btn, bool pressed);
};

} // namespace hyperion

#endif
