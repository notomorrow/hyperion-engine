/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <input/Keyboard.hpp>
#include <input/Mouse.hpp>

#include <core/math/Vector2.hpp>

#include <core/object/HypObject.hpp>

#include <core/memory/RefCountedPtr.hpp>
#include <core/memory/Pimpl.hpp>

namespace hyperion {

struct InputState;

HYP_CLASS(Abstract)
class HYP_API InputHandlerBase : public HypObjectBase
{
    HYP_OBJECT_BODY(InputHandlerBase);

public:
    InputHandlerBase();
    InputHandlerBase(const InputHandlerBase& other) = delete;
    InputHandlerBase& operator=(const InputHandlerBase& other) = delete;
    InputHandlerBase(InputHandlerBase&& other) noexcept = delete;
    InputHandlerBase& operator=(InputHandlerBase&& other) noexcept = delete;
    virtual ~InputHandlerBase();
    
    HYP_FORCE_INLINE void SetDeltaTime(float deltaTime)
    {
        m_deltaTime = deltaTime;
    }

    HYP_METHOD(Scriptable)
    bool OnKeyDown(const KeyboardEvent& evt);

    HYP_METHOD(Scriptable)
    bool OnKeyUp(const KeyboardEvent& evt);

    HYP_METHOD(Scriptable)
    bool OnMouseDown(const MouseEvent& evt);

    HYP_METHOD(Scriptable)
    bool OnMouseUp(const MouseEvent& evt);

    HYP_METHOD(Scriptable)
    bool OnMouseMove(const MouseEvent& evt);

    HYP_METHOD(Scriptable)
    bool OnMouseDrag(const MouseEvent& evt);
    
    HYP_METHOD(Scriptable)
    bool OnMouseLeave(const MouseEvent& evt);

    HYP_METHOD(Scriptable)
    bool OnClick(const MouseEvent& evt);
    
    const Bitset& GetKeyStates() const;

    bool IsKeyDown(KeyCode key) const;
    bool IsKeyUp(KeyCode key) const;
    
    bool IsMouseButtonDown(MouseButton btn) const;
    bool IsMouseButtonUp(MouseButton btn) const;

protected:
    HYP_METHOD()
    virtual bool OnKeyDown_Impl(const KeyboardEvent& evt);

    HYP_METHOD()
    virtual bool OnKeyUp_Impl(const KeyboardEvent& evt);

    HYP_METHOD()
    virtual bool OnMouseDown_Impl(const MouseEvent& evt);

    HYP_METHOD()
    virtual bool OnMouseUp_Impl(const MouseEvent& evt);

    HYP_METHOD()
    virtual bool OnMouseMove_Impl(const MouseEvent& evt) = 0;

    HYP_METHOD()
    virtual bool OnMouseDrag_Impl(const MouseEvent& evt) = 0;
    
    HYP_METHOD()
    virtual bool OnMouseLeave_Impl(const MouseEvent& evt) = 0;

    HYP_METHOD()
    virtual bool OnClick_Impl(const MouseEvent& evt) = 0;

private:
    Pimpl<InputState> m_inputState;
    
protected:
    float m_deltaTime;
};

HYP_CLASS()
class NullInputHandler final : public InputHandlerBase
{
    HYP_OBJECT_BODY(NullInputHandler);

public:
    NullInputHandler() = default;
    virtual ~NullInputHandler() override = default;

private:
    void Init() override
    {
        SetReady(true);
    }

    virtual bool OnKeyDown_Impl(const KeyboardEvent& evt) override
    {
        return false;
    }

    virtual bool OnKeyUp_Impl(const KeyboardEvent& evt) override
    {
        return false;
    }

    virtual bool OnMouseDown_Impl(const MouseEvent& evt) override
    {
        return false;
    }

    virtual bool OnMouseUp_Impl(const MouseEvent& evt) override
    {
        return false;
    }

    virtual bool OnMouseMove_Impl(const MouseEvent& evt) override
    {
        return false;
    }

    virtual bool OnMouseDrag_Impl(const MouseEvent& evt) override
    {
        return false;
    }
    
    virtual bool OnMouseLeave_Impl(const MouseEvent& evt) override
    {
        return false;
    }

    virtual bool OnClick_Impl(const MouseEvent& evt) override
    {
        return false;
    }
};

} // namespace hyperion

