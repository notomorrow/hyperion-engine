#pragma once
#include <core/math/Vector2.hpp>
#include <core/utilities/EnumFlags.hpp>

namespace hyperion {

class InputManager;

HYP_ENUM()
enum class MouseButton : uint32
{
    INVALID = ~0u,
    LEFT = 0,
    MIDDLE,
    RIGHT,

    MAX
};

HYP_ENUM()
enum class MouseButtonState : uint32
{
    NONE = 0x0,
    LEFT = 1u << uint32(MouseButton::LEFT),
    MIDDLE = 1u << uint32(MouseButton::MIDDLE),
    RIGHT = 1u << uint32(MouseButton::RIGHT)
};

HYP_MAKE_ENUM_FLAGS(MouseButtonState)

HYP_STRUCT(Size = 56)
struct MouseEvent
{
    HYP_FIELD()
    InputManager* inputManager = nullptr;

    HYP_FIELD()
    Vec2f position;

    HYP_FIELD()
    Vec2f previousPosition;

    HYP_FIELD()
    Vec2i absolutePosition;

    HYP_FIELD()
    EnumFlags<MouseButtonState> mouseButtons = MouseButtonState::NONE;

    HYP_FIELD()
    Vec2i wheel;

    HYP_FIELD(Deprecated)
    bool isDown = false;
};

struct InputMouseLockState
{
    InputManager* inputManager = nullptr;
    bool locked = false;

    HYP_FORCE_INLINE bool operator==(const InputMouseLockState& other) const
    {
        return inputManager == other.inputManager
            && locked == other.locked;
    }

    HYP_FORCE_INLINE bool operator!=(const InputMouseLockState& other) const
    {
        return inputManager != other.inputManager
            || locked != other.locked;
    }
};

struct InputMouseLockScope
{
    InputMouseLockState* mouseLockState;

    InputMouseLockScope()
        : mouseLockState(nullptr)
    {
    }

    InputMouseLockScope(InputMouseLockState& mouseLockState)
        : mouseLockState(&mouseLockState)
    {
    }

    InputMouseLockScope(const InputMouseLockScope& other) = delete;
    InputMouseLockScope& operator=(const InputMouseLockScope& other) = delete;

    InputMouseLockScope(InputMouseLockScope&& other) noexcept
        : mouseLockState(other.mouseLockState)
    {
    }

    HYP_API InputMouseLockScope& operator=(InputMouseLockScope&& other) noexcept;

    HYP_API ~InputMouseLockScope();

    HYP_API void Reset();

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return mouseLockState != nullptr && mouseLockState->locked;
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return mouseLockState == nullptr || !mouseLockState->locked;
    }
};

} // namespace hyperion
