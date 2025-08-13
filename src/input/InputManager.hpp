/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#define NUM_KEYBOARD_KEYS 350
#define NUM_MOUSE_BUTTONS 3

#include <core/Defines.hpp>

#include <core/object/HypObject.hpp>

#include <core/containers/FlatMap.hpp>
#include <core/containers/Bitset.hpp>
#include <core/containers/LinkedList.hpp>

#include <core/containers/Bitset.hpp>

#include <core/math/Vector2.hpp>

#include <core/threading/Semaphore.hpp>

#include <input/Keyboard.hpp>
#include <input/Mouse.hpp>

namespace hyperion {

namespace sys {
class ApplicationWindow;
class SystemEvent;
} // namespace sys

using sys::ApplicationWindow;
using sys::SystemEvent;

struct InputState
{
    Bitset keyStates;
    EnumFlags<MouseButtonState> mouseButtonStates;

    InputState()
        : keyStates {},
          mouseButtonStates {}
    {
    }
};

class InputEventNotifier final : public Semaphore<int32, SemaphoreDirection::WAIT_FOR_POSITIVE, threading::AtomicSemaphoreImpl<int32, SemaphoreDirection::WAIT_FOR_POSITIVE>>
{
};

class HYP_API InputEventSink
{
public:
    InputEventSink();
    InputEventSink(const InputEventSink& other) = delete;
    InputEventSink& operator=(const InputEventSink& other) = delete;
    InputEventSink(InputEventSink&& other) noexcept = delete;
    InputEventSink& operator=(InputEventSink&& other) noexcept = delete;
    ~InputEventSink();

    void Push(SystemEvent&& evt);
    bool Poll(Array<SystemEvent>& outEvents);

private:
    InputEventNotifier m_notifier;
    Array<SystemEvent, DynamicAllocator> m_events;
    volatile int64 m_lockState;
};

HYP_CLASS()
class InputManager : public HypObjectBase
{
    HYP_OBJECT_BODY(InputManager);

    friend struct InputMouseLockScope;

public:
    HYP_API InputManager();
    InputManager(const InputManager& other) = delete;
    InputManager& operator=(const InputManager& other) = delete;
    InputManager(InputManager&& other) noexcept = delete;
    InputManager& operator=(InputManager&& other) noexcept = delete;
    HYP_API ~InputManager();

    HYP_API void CheckEvent(SystemEvent* event);

    HYP_METHOD()
    HYP_API bool IsMouseLocked() const;

    HYP_METHOD()
    HYP_API void PushMouseLockState(bool mouseLocked);

    HYP_METHOD()
    HYP_API void PopMouseLockState();

    HYP_API InputMouseLockScope AcquireMouseLock();

    HYP_METHOD()
    const Vec2i& GetMousePosition() const
    {
        return m_mousePosition;
    }

    HYP_METHOD()
    HYP_API void SetMousePosition(Vec2i position);

    const Vec2i& GetPreviousMousePosition() const
    {
        return m_previousMousePosition;
    }

    HYP_METHOD()
    const Vec2i& GetWindowSize() const
    {
        return m_windowSize;
    }

    void KeyDown(KeyCode key)
    {
        SetKey(key, true);
    }

    void KeyUp(KeyCode key)
    {
        SetKey(key, false);
    }

    void MouseButtonDown(MouseButton btn)
    {
        SetMouseButton(btn, true);
    }

    void MouseButtonUp(MouseButton btn)
    {
        SetMouseButton(btn, false);
    }

    HYP_METHOD()
    HYP_API bool IsKeyDown(KeyCode key) const;

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsKeyUp(KeyCode key) const
    {
        return !IsKeyDown(key);
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsShiftDown() const
    {
        return IsKeyDown(KeyCode::LEFT_SHIFT) || IsKeyDown(KeyCode::RIGHT_SHIFT);
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsAltDown() const
    {
        return IsKeyDown(KeyCode::LEFT_ALT) || IsKeyDown(KeyCode::RIGHT_ALT);
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsCtrlDown() const
    {
        return IsKeyDown(KeyCode::LEFT_CTRL) || IsKeyDown(KeyCode::RIGHT_CTRL);
    }

    HYP_METHOD()
    HYP_API bool IsButtonDown(MouseButton btn) const;

    HYP_API EnumFlags<MouseButtonState> GetButtonStates() const;

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsButtonUp(MouseButton btn) const
    {
        return !IsButtonDown(btn);
    }

    HYP_FORCE_INLINE ApplicationWindow* GetWindow() const
    {
        return m_window;
    }

    HYP_FORCE_INLINE void SetWindow(ApplicationWindow* window)
    {
        m_window = window;
    }

private:
    void SetIsMouseLocked(bool isMouseLocked);

    void UpdateMousePosition();
    void UpdateWindowSize(Vec2i newSize);

    void SetKey(KeyCode key, bool pressed);
    void SetMouseButton(MouseButton btn, bool pressed);

    void ApplyMouseLockState(const InputMouseLockState* mouseLockState);
    void RemoveMouseLockState(const InputMouseLockState* mouseLockState);

    InputState m_inputState;
    Vec2i m_mousePosition;
    Vec2i m_previousMousePosition;
    Vec2i m_windowSize;
    bool m_isMouseLocked;

    LinkedList<InputMouseLockState> m_mouseLockStates;
    Mutex m_mouseLockStatesMutex;

    ApplicationWindow* m_window;
};

} // namespace hyperion

