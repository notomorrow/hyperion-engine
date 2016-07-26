#include "input_manager.h"
#include "core_engine.h"
#include <iostream>

namespace apex {

InputEvent::InputEvent()
    : is_up_evt(false), is_empty(true)
{
}

InputEvent::InputEvent(std::function<void()> handler, bool is_up_evt)
    : handler(handler), is_up_evt(is_up_evt), is_empty(false)
{
}

InputEvent::InputEvent(const InputEvent &other)
    : handler(other.handler), is_up_evt(other.is_up_evt), is_empty(other.is_empty)
{
}

bool InputEvent::IsEmpty() const
{
    return is_empty;
}

bool InputEvent::IsUpEvent() const
{
    return is_up_evt;
}

void InputEvent::Trigger()
{
    handler();
}

InputManager::InputManager()
{
    key_events = new InputEvent[NUM_KEYBOARD_KEYS];
    mouse_events = new InputEvent[NUM_MOUSE_BUTTONS];

    memset(key_states, false, NUM_KEYBOARD_KEYS);
    memset(mouse_states, false, NUM_MOUSE_BUTTONS);

    mouse_x = 0;
    mouse_y = 0;
}

InputManager::~InputManager()
{
    delete[] key_events;
    delete[] mouse_events;
}

double InputManager::GetMouseX() const
{
    return mouse_x;
}

double InputManager::GetMouseY() const
{
    return mouse_y;
}

void InputManager::SetMousePosition(double x, double y)
{
    CoreEngine::GetInstance()->SetMousePosition(x, y);
}

void InputManager::KeyDown(int key)
{
    if (key > 0 && key < NUM_KEYBOARD_KEYS) {
        InputEvent &handler = key_events[key];
        if (!handler.IsEmpty() && !handler.IsUpEvent()) {
            handler.Trigger();
        }
        key_states[key] = true;
    }
}

void InputManager::KeyUp(int key)
{
    if (key > 0 && key < NUM_KEYBOARD_KEYS) {
        InputEvent &handler = key_events[key];
        if (!handler.IsEmpty() && handler.IsUpEvent()) {
            handler.Trigger();
        }
        key_states[key] = false;
    }
}

void InputManager::MouseButtonDown(int btn)
{
    if (btn > 0 && btn < NUM_MOUSE_BUTTONS) {
        InputEvent &handler = mouse_events[btn];
        if (!handler.IsEmpty() && !handler.IsUpEvent()) {
            handler.Trigger();
        }
        mouse_states[btn] = true;
    }
}

void InputManager::MouseButtonUp(int btn)
{
    if (btn > 0 && btn < NUM_MOUSE_BUTTONS) {
        InputEvent &handler = mouse_events[btn];
        if (!handler.IsEmpty() && handler.IsUpEvent()) {
            handler.Trigger();
        }
        mouse_states[btn] = true;
    }
}

void InputManager::MouseMove(double x, double y)
{
    mouse_x = x;
    mouse_y = y;
}

bool InputManager::IsKeyDown(int key) const
{
    if (key > 0 && key < NUM_KEYBOARD_KEYS) {
        return key_states[key];
    }
    return false;
}

bool InputManager::IsKeyUp(int key) const
{
    return !IsKeyDown(key);
}

bool InputManager::IsButtonDown(int btn) const
{
    if (btn > 0 && btn < NUM_MOUSE_BUTTONS) {
        return mouse_states[btn];
    }
    return false;
}

bool InputManager::IsButtonUp(int btn) const
{
    return !IsButtonDown(btn);
}

bool InputManager::RegisterKeyEvent(int key, const InputEvent &evt)
{
    if (key > 0 && key < NUM_KEYBOARD_KEYS) {
        key_events[key] = evt;
        return true;
    }
    return false;
}

bool InputManager::RegisterClickEvent(int btn, const InputEvent &evt)
{
    if (btn > 0 && btn < NUM_MOUSE_BUTTONS) {
        mouse_events[btn] = evt;
        return true;
    }
    return false;
}
}