global using LayerId = System.UInt32;

using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "LayersMask")]
    [StructLayout(LayoutKind.Explicit, Size = 8)]
    public struct LayersMask
    {
        [FieldOffset(0)]
        public ulong mask;

        public LayersMask(ulong mask)
        {
            this.mask = mask;
        }

        public static LayersMask operator &(LayersMask a, LayersMask b)
        {
            return new LayersMask(a.mask & b.mask);
        }

        public static LayersMask operator |(LayersMask a, LayersMask b)
        {
            return new LayersMask(a.mask | b.mask);
        }
    }

    [ClassBinding(Name="Layer")]
    public class Layer : ObjectBase
    {
        public Layer()
        {
        }
    }
}