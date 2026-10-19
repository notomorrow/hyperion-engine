using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    /// Wrapper struct corresponding to /core/memory/Pimpl.hpp in the C++ code
    [StructLayout(LayoutKind.Sequential)]
    public struct PimplPtr
    {
        internal IntPtr ptr;

        public PimplPtr()
        {
            ptr = IntPtr.Zero;
        }
    }
}