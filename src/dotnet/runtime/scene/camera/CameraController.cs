using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="CameraController")]
    public class CameraController : HypObject
    {
        public CameraController()
        {
        }
    }

    [HypClassBinding(Name="OrthoCameraController")]
    public class OrthoCameraController : CameraController
    {
        public OrthoCameraController()
        {
        }
    }

    [HypClassBinding(Name="PerspectiveCameraController")]
    public class PerspectiveCameraController : CameraController
    {
        public PerspectiveCameraController()
        {
        }
    }

    [HypClassBinding(Name="FirstPersonCameraControllerMode")]
    public enum FirstPersonCameraControllerMode : uint
    {
        MouseLocked = 0,
        MouseFree = 1
    }

    [HypClassBinding(Name="FollowCameraController")]
    public class FollowCameraController : PerspectiveCameraController
    {
        public FollowCameraController()
        {
        }
    }

    [HypClassBinding(Name="FirstPersonCameraController")]
    public class FirstPersonCameraController : PerspectiveCameraController
    {
        public FirstPersonCameraController()
        {
        }
    }

    [HypClassBinding(Name="CameraTrackController")]
    public class CameraTrackController : PerspectiveCameraController
    {
        public CameraTrackController()
        {
        }
    }
}