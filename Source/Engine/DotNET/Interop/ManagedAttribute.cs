using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [StructLayout(LayoutKind.Sequential)]
    public struct ManagedAttribute
    {
        public IntPtr pClass;

        public ObjectReference objectReference;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct ManagedAttributeHolder : IDisposable
    {
        public uint managedAttributesSize;

        public IntPtr managedAttributesPtr;

        public void Dispose()
        {
            if (managedAttributesPtr != IntPtr.Zero)
            {
                Marshal.FreeHGlobal(managedAttributesPtr);
            }
        }
    }
}