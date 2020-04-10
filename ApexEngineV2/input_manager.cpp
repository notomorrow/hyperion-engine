#include "input_manager.h"
#include <iostream>
#include <string.h>

namespace apex {

InputEvent::InputEvent()
    : is_up_evt(false), 
      is_empty(true)
{
}

InputEvent::InputEvent(std::function<void()> handler, bool is_up_evt)
    : handler(handler), 
      is_up_evt(is_up_evt), 
      is_empty(false)
{
}

InputEvent::InputEvent(const InputEvent &other)
    : handler(other.handler), 
      is_up_evt(other.is_up_evt), 
      is_empty(other.is_empty)
{
}

InputManager::InputManager()
{
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

void InputManager::SetKey(int key, bool pressed)
{
    if (key >= 0 && key < NUM_KEYBOARD_KEYS) {
        InputEvent &handler = key_events[key];

        if (!handler.IsEmpty() && (pressed ? handler.IsUpEvent() : !handler.IsUpEvent())) {
            handler.Trigger();
        }

        key_states[key] = pressed;
    }
}

void InputManager::SetMouseButton(int btn, bool pressed)
{
    if (btn >= 0 && btn < NUM_MOUSE_BUTTONS) {
        InputEvent &handler = mouse_events[btn];

        if (!handler.IsEmpty() && (pressed ? handler.IsUpEvent() : !handler.IsUpEvent())) {
            handler.Trigger();
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

} // namespace apex
