using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="CameraProjectionMode")]
    public enum CameraProjectionMode : uint
    {
        None = 0,
        Perspective = 1,
        Orthographic = 2
    }

    [HypClassBinding(Name="CameraFlags")]
    [Flags]
    public enum CameraFlags : uint
    {
        None = 0x0,
        MatchWindowSize = 0x1
    }

    [HypClassBinding(Name="Camera")]
    public class Camera : HypObject
    {
        public Camera()
        {
        }
    }
}