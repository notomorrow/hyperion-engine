/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CAMERA_TRACK_CONTROLLER_HPP
#define HYPERION_CAMERA_TRACK_CONTROLLER_HPP

#include <core/memory/RefCountedPtr.hpp>

#include <scene/camera/PerspectiveCamera.hpp>
#include <scene/camera/CameraTrack.hpp>

namespace hyperion {

HYP_CLASS()
class CameraTrackController : public PerspectiveCameraController
{
    HYP_OBJECT_BODY(CameraTrackController);

public:
    CameraTrackController();
    CameraTrackController(RC<CameraTrack> cameraTrack);
    virtual ~CameraTrackController() = default;

    const RC<CameraTrack>& GetCameraTrack() const
    {
        return m_cameraTrack;
    }

    void SetCameraTrack(RC<CameraTrack> cameraTrack)
    {
        m_cameraTrack = std::move(cameraTrack);
    }

    virtual void UpdateLogic(double dt) override;

protected:
    RC<CameraTrack> m_cameraTrack;
    double m_trackTime;

private:
    virtual void RespondToCommand(const CameraCommand& command, float dt) override;
};

} // namespace hyperion

#endif
