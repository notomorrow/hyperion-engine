/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FOLLOW_CAMERA_HPP
#define HYPERION_FOLLOW_CAMERA_HPP

#include <scene/camera/PerspectiveCamera.hpp>

namespace hyperion {

HYP_CLASS()
class HYP_API FollowCameraController : public PerspectiveCameraController
{
    HYP_OBJECT_BODY(FollowCameraController);

public:
    FollowCameraController(const Vector3& target, const Vector3& offset);
    virtual ~FollowCameraController() override = default;

    const Vector3& GetOffset() const
    {
        return m_offset;
    }

    void SetOffset(const Vector3& offset)
    {
        m_offset = offset;
    }

    virtual void UpdateLogic(double dt) override;

protected:
    virtual void OnActivated() override;
    virtual void OnDeactivated() override;

private:
    virtual void RespondToCommand(const CameraCommand& command, float dt) override;

    Vec3f m_offset,
        m_realOffset;

    Vec3f m_target;

    float m_mx,
        m_my,
        m_prevMx,
        m_prevMy,
        m_desiredDistance;

    Vec2f m_mag,
        m_prevMag;
};

} // namespace hyperion

#endif
