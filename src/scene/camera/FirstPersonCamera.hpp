/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scene/camera/PerspectiveCamera.hpp>

#include <core/math/Vector2.hpp>

namespace hyperion {

class FirstPersonCameraController;

HYP_ENUM()
enum class FirstPersonCameraControllerMode : uint32
{
    MOUSE_LOCKED = 0,
    MOUSE_FREE = 1
};

HYP_CLASS()
class HYP_API FirstPersonCameraInputHandler : public InputHandlerBase
{
    HYP_OBJECT_BODY(FirstPersonCameraInputHandler);

public:
    FirstPersonCameraInputHandler(const WeakHandle<CameraController>& controller);
    virtual ~FirstPersonCameraInputHandler() override = default;

protected:
    virtual bool OnKeyDown_Impl(const KeyboardEvent& evt) override;
    virtual bool OnKeyUp_Impl(const KeyboardEvent& evt) override;
    virtual bool OnMouseDown_Impl(const MouseEvent& evt) override;
    virtual bool OnMouseUp_Impl(const MouseEvent& evt) override;
    virtual bool OnMouseMove_Impl(const MouseEvent& evt) override;
    virtual bool OnMouseDrag_Impl(const MouseEvent& evt) override;
    virtual bool OnMouseLeave_Impl(const MouseEvent& evt) override;
    virtual bool OnClick_Impl(const MouseEvent& evt) override;

private:
    WeakHandle<FirstPersonCameraController> m_controller;
};

HYP_CLASS()
class HYP_API FirstPersonCameraController : public PerspectiveCameraController
{
    HYP_OBJECT_BODY(FirstPersonCameraController);

public:
    FirstPersonCameraController(FirstPersonCameraControllerMode mode = FirstPersonCameraControllerMode::MOUSE_FREE);
    virtual ~FirstPersonCameraController() = default;

    HYP_METHOD(Property = "Mode")
    FirstPersonCameraControllerMode GetMode() const
    {
        return m_mode;
    }

    HYP_METHOD(Property = "Mode")
    void SetMode(FirstPersonCameraControllerMode mode);

    HYP_METHOD()
    virtual bool IsMouseLockAllowed() const override
    {
        return true;
    }

    virtual void UpdateLogic(double dt) override;

protected:
    virtual void Init() override;

    virtual void OnActivated() override;
    virtual void OnDeactivated() override;

    virtual void RespondToCommand(const CameraCommand& command, float dt) override;

    FirstPersonCameraControllerMode m_mode;

    Vec3f m_moveDeltas;
    Vec3f m_dirCrossY;

    float m_mouseX;
    float m_mouseY;
    float m_prevMouseX;
    float m_prevMouseY;

    Vec2f m_mag;
    Vec2f m_desiredMag;
    Vec2f m_prevMag;
};

} // namespace hyperion

