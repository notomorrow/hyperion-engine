/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef INPUT_MANAGER_HPP
#define INPUT_MANAGER_HPP

#define NUM_KEYBOARD_KEYS 350
#define NUM_MOUSE_BUTTONS 3

#include <core/containers/FlatMap.hpp>
#include <core/containers/Bitset.hpp>
#include <core/system/AppContext.hpp>

#include <math/Vector2.hpp>

#include <input/Keyboard.hpp>

#include <atomic>

namespace hyperion {

template <bool IsAtomic>
struct Scalar2D { };

template<>
struct Scalar2D<true>
{
    std::atomic<int> x { 0 };
    std::atomic<int> y { 0 };

    int GetX() const
        { return x.load(std::memory_order_relaxed); }

    int GetY() const
        { return y.load(std::memory_order_relaxed); }

    explicit operator Vector2() const
        { return Vector2(float(GetX()), float(GetY())); }
};

template<>
struct Scalar2D<false>
{
    int x { 0 };
    int y { 0 };

    Scalar2D() = default;

    Scalar2D(int x, int y)
    {
        Scalar2D::x = x;
        Scalar2D::y = y;
    }

    Scalar2D(const Scalar2D &other) = default;
    Scalar2D &operator=(const Scalar2D &other) = default;
    Scalar2D(Scalar2D &&other) noexcept = default;
    Scalar2D &operator=(Scalar2D &other) noexcept = default;

    int GetX() const
        { return x; }

    int GetY() const
        { return y; }

    explicit operator Vector2() const
        { return Vector2(float(GetX()), float(GetY())); }
};

struct InputState
{
    bool    key_states[NUM_KEYBOARD_KEYS];
    bool    last_key_states[NUM_KEYBOARD_KEYS];

    bool    mouse_button_states[int(MouseButton::MAX)];

    InputState()
        : key_states { false },
          last_key_states { false },
          mouse_button_states { false }
    {
    }
};

class InputManager
{
public:
    HYP_API InputManager();
    InputManager(const InputManager &other)                 = delete;
    InputManager &operator=(const InputManager &other)      = delete;
    InputManager(InputManager &&other) noexcept             = delete;
    InputManager &operator=(InputManager &&other) noexcept  = delete;
    HYP_API ~InputManager();

    HYP_API void CheckEvent(SystemEvent *event);

    const Vec2i &GetMousePosition() const
        { return m_mouse_position; }

    HYP_API void SetMousePosition(Vec2i position);

    const Vec2i &GetPreviousMousePosition() const
        { return m_previous_mouse_position; }

    Scalar2D<true> &GetWindowSize()
        { return m_window_size; }

    const Scalar2D<true> &GetWindowSize() const
        { return m_window_size; }

    void KeyDown(KeyCode key)
        { SetKey(key, true); }

    void KeyUp(KeyCode key)
        { SetKey(key, false); }

    void MouseButtonDown(MouseButton btn)
        { SetMouseButton(btn, true); }

    void MouseButtonUp(MouseButton btn)
        { SetMouseButton(btn, false); }

    HYP_API bool IsKeyDown(KeyCode key) const;

    bool IsKeyUp(KeyCode key) const
        { return !IsKeyDown(key); }

    HYP_API bool IsButtonDown(MouseButton btn) const;

    HYP_API EnumFlags<MouseButtonState> GetButtonStates() const;

    bool IsButtonUp(MouseButton btn) const
        { return !IsButtonDown(btn); }

    ApplicationWindow *GetWindow() const
        { return m_window; }

    void SetWindow(ApplicationWindow *window)
        { m_window = window; }

private:
    void UpdateMousePosition();
    void UpdateWindowSize(Vec2u new_size);

    void SetKey(KeyCode key, bool pressed);
    void SetMouseButton(MouseButton btn, bool pressed);

    InputState          m_input_state;
    Vec2i               m_mouse_position;
    Vec2i               m_previous_mouse_position;
    Scalar2D<true>      m_window_size;

    ApplicationWindow   *m_window;
};

} // namespace hyperion

#endif
