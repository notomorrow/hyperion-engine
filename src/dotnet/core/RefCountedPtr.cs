using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    /// <summary>
    ///  Wrapper for RefCountedPtr.hpp from the core library
    /// </summary>
    [StructLayout(LayoutKind.Sequential, Size = 8)]
    public struct RefCountedPtr
    {
        public static readonly RefCountedPtr Null = new RefCountedPtr(IntPtr.Zero);

        private IntPtr ctrlBlock;

        public RefCountedPtr(IntPtr ctrlBlock)
        {
            this.ctrlBlock = ctrlBlock;
        }
    }

    [StructLayout(LayoutKind.Sequential, Size = 8)]
    public struct WeakRefCountedPtr
    {
        public static readonly WeakRefCountedPtr Null = new WeakRefCountedPtr(IntPtr.Zero);

        private IntPtr ctrlBlock;

        public WeakRefCountedPtr(IntPtr ctrlBlock)
        {
            this.ctrlBlock = ctrlBlock;
        }
    }
}
