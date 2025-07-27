using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="ShadowMapFilter")]
    public enum ShadowMapFilter : uint
    {
        Standard = 0,
        Pcf,
        ContactHardened,
        Vsm,

        Count
    }

    [HypClassBinding(Name="ShadowMapType")]
    public enum ShadowMapType : uint
    {
        Directional = 0,
        Spot,
        Omni,
        Count
    }
}