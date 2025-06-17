/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <input/InputHandler.hpp>
#include <input/InputManager.hpp>

#include <core/utilities/ByteUtil.hpp>

namespace hyperion {

#pragma region InputHandlerBase

InputHandlerBase::InputHandlerBase()
    : m_inputState(MakePimpl<InputState>())
{
}

InputHandlerBase::~InputHandlerBase()
{
}

bool InputHandlerBase::OnKeyDown_Impl(const KeyboardEvent& evt)
{
    if (uint32(evt.keyCode) < NUM_KEYBOARD_KEYS)
    {
        m_inputState->keyStates[uint32(evt.keyCode)] = true;
    }

    return true;
}

bool InputHandlerBase::OnKeyUp_Impl(const KeyboardEvent& evt)
{
    if (uint32(evt.keyCode) < NUM_KEYBOARD_KEYS)
    {
        m_inputState->keyStates[uint32(evt.keyCode)] = false;
    }

    return true;
}

bool InputHandlerBase::OnMouseDown_Impl(const MouseEvent& evt)
{
    FOR_EACH_BIT(uint32(evt.mouseButtons), i)
    {
        if (i < NUM_MOUSE_BUTTONS)
        {
            m_inputState->mouseButtonStates[i] = true;
        }
    }

    return true;
}

bool InputHandlerBase::OnMouseUp_Impl(const MouseEvent& evt)
{
    FOR_EACH_BIT(uint32(evt.mouseButtons), i)
    {
        if (i < NUM_MOUSE_BUTTONS)
        {
            m_inputState->mouseButtonStates[i] = false;
        }
    }

    return true;
}

bool InputHandlerBase::IsKeyDown(KeyCode key) const
{
    if (uint32(key) < NUM_KEYBOARD_KEYS)
    {
        return m_inputState->keyStates[uint32(key)];
    }

    return false;
}

bool InputHandlerBase::IsKeyUp(KeyCode key) const
{
    if (uint32(key) < NUM_KEYBOARD_KEYS)
    {
        return !m_inputState->keyStates[uint32(key)];
    }

    return false;
}

bool InputHandlerBase::IsMouseButtonDown(MouseButton btn) const
{
    if (uint32(btn) < NUM_MOUSE_BUTTONS)
    {
        return m_inputState->mouseButtonStates[uint32(btn)];
    }

    return false;
}

bool InputHandlerBase::IsMouseButtonUp(MouseButton btn) const
{
    if (uint32(btn) < NUM_MOUSE_BUTTONS)
    {
        return !m_inputState->mouseButtonStates[uint32(btn)];
    }

    return false;
}

#pragma endregion InputHandlerBase

} // namespace hyperion
