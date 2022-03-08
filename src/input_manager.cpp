#include "input_manager.h"
#include <iostream>
#include <string.h>

namespace hyperion {

InputEvent::InputEvent()
    : m_is_empty(true)
{
}

InputEvent::InputEvent(std::function<void(bool)> handler)
    : m_handler(handler),
      m_is_empty(false)
{
}

InputEvent::InputEvent(const InputEvent &other)
    : m_handler(other.m_handler),
      m_is_empty(other.m_is_empty)
{
}

InputManager::InputManager(SystemWindow *window)
{
    this->window = window;

    key_events = new InputEvent[NUM_KEYBOARD_KEYS];
    mouse_events = new InputEvent[NUM_MOUSE_BUTTONS];

    memset(key_states, false, NUM_KEYBOARD_KEYS * sizeof(bool));
    memset(mouse_states, false, NUM_MOUSE_BUTTONS * sizeof(bool));

    mouse_x = 0.0;
    mouse_y = 0.0;
}

InputManager::~InputManager()
{
    delete[] key_events;
    delete[] mouse_events;
}

void InputManager::CheckEvent(SystemEvent *event) {
    switch (event->GetType()) {
        DebugLog(LogType::Debug, "Recv event %d\n", event->GetType());
        case SystemEventType::EVENT_KEYDOWN:
            this->KeyDown(event->GetKeyCode());
            break;
        case SystemEventType::EVENT_KEYUP:
            this->KeyUp(event->GetKeyCode());
            break;
        case SystemEventType::EVENT_MOUSEBUTTON_DOWN:
            this->MouseButtonDown(event->GetMouseButton());
            break;
        case SystemEventType::EVENT_MOUSEBUTTON_UP:
            this->MouseButtonUp(event->GetMouseButton());
            break;
        case SystemEventType::EVENT_MOUSEMOTION:
            this->UpdateMousePosition();
            break;
        default:
            break;
    }
}

void InputManager::UpdateMousePosition() {
    this->GetMousePosition((int *)&this->mouse_x, (int *)&this->mouse_y);
}

void InputManager::SetKey(int key, bool pressed)
{
    if (key >= 0 && key < NUM_KEYBOARD_KEYS) {
        /* Set all letters to uppercase */
        if (key >= 'a' && key <= 'z')
            key = 'A'+(key-'a');

        InputEvent &handler = key_events[key];

        if (!handler.IsEmpty()) {
            handler.Trigger(pressed);
        }

        key_states[key] = pressed;
    }
}

void InputManager::SetMouseButton(int btn, bool pressed)
{
    if (btn >= 0 && btn < NUM_MOUSE_BUTTONS) {
        InputEvent &handler = mouse_events[btn];

        if (!handler.IsEmpty()) {
            handler.Trigger(pressed);
        }

        mouse_states[btn] = pressed;
    }
}

bool InputManager::IsKeyDown(int key) const
{
    if (key >= 0 && key < NUM_KEYBOARD_KEYS) {
        return key_states[key];
    }
    return false;
}

bool InputManager::IsButtonDown(int btn) const
{
    if (btn >= 0 && btn < NUM_MOUSE_BUTTONS) {
        return mouse_states[btn];
    }
    return false;
}

bool InputManager::RegisterKeyEvent(int key, const InputEvent &evt)
{
    DebugLog(LogType::Debug, "Registering key event for [%d]\n", key);
    if (key >= 0 && key < NUM_KEYBOARD_KEYS) {
        key_events[key] = evt;
        return true;
    }
    return false;
}

bool InputManager::RegisterClickEvent(int btn, const InputEvent &evt)
{
    if (btn >= 0 && btn < NUM_MOUSE_BUTTONS) {
        mouse_events[btn] = evt;
        return true;
    }
    return false;
}

} // namespace hyperion
