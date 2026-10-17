using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "EditorViewportType")]
    public enum EditorViewportType : byte
    {
        Perspective = 0,
        Top = 1,
        Front = 2,
        Side = 3
    }

    [ClassBinding(Name = "EditorViewport")]
    public class EditorViewport : ObjectBase
    {
        public EditorViewport()
        {
        }
    }
}