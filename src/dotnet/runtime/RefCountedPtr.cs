using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public static class RefCountedPtrNativeBindings
    {
        [DllImport("hyperion", EntryPoint = "RefCountedPtr_IncRef")]
        internal static extern void RefCountedPtr_IncRef(IntPtr ctrlBlock, IntPtr address);

        [DllImport("hyperion", EntryPoint = "RefCountedPtr_DecRef")]
        internal static extern void RefCountedPtr_DecRef(IntPtr ctrlBlock, IntPtr address);

        [DllImport("hyperion", EntryPoint = "RefCountedPtr_Get")]
        internal static extern void RefCountedPtr_Get(IntPtr ctrlBlock, IntPtr address, [Out] out HypDataBuffer outHypDataBuffer);

        [DllImport("hyperion", EntryPoint = "WeakRefCountedPtr_IncRef")]
        internal static extern void WeakRefCountedPtr_IncRef(IntPtr ctrlBlock, IntPtr address);

        [DllImport("hyperion", EntryPoint = "WeakRefCountedPtr_DecRef")]
        internal static extern void WeakRefCountedPtr_DecRef(IntPtr ctrlBlock, IntPtr address);

        [DllImport("hyperion", EntryPoint = "WeakRefCountedPtr_Lock")]
        internal static extern uint WeakRefCountedPtr_Lock(IntPtr ctrlBlock, IntPtr address);
    }

    [StructLayout(LayoutKind.Sequential, Size = 16)]
    public struct RefCountedPtr
    {
        public static readonly RefCountedPtr Null = new RefCountedPtr(IntPtr.Zero, IntPtr.Zero);

        private IntPtr ctrlBlock;
        private IntPtr ptr;

        public RefCountedPtr(IntPtr ctrlBlock, IntPtr ptr)
        {
            this.ctrlBlock = ctrlBlock;
            this.ptr = ptr;
        }

        public bool IsValid
        {
            get
            {
                return ctrlBlock != IntPtr.Zero;
            }
        }

        public void IncRef()
        {
            RefCountedPtrNativeBindings.RefCountedPtr_IncRef(ctrlBlock, ptr);
        }

        public void DecRef()
        {
            RefCountedPtrNativeBindings.RefCountedPtr_DecRef(ctrlBlock, ptr);
        }
    }

    [StructLayout(LayoutKind.Sequential, Size = 16)]
    public struct RefCountedPtr<T>
    {
        public static readonly RefCountedPtr<T> Null = new RefCountedPtr<T>(IntPtr.Zero, IntPtr.Zero);

        private IntPtr ctrlBlock;
        private IntPtr ptr;

        public RefCountedPtr()
        {
            this.ctrlBlock = IntPtr.Zero;
            this.ptr = IntPtr.Zero;
        }

        public RefCountedPtr(IntPtr ctrlBlock, IntPtr ptr)
        {
            this.ctrlBlock = ctrlBlock;
            this.ptr = ptr;
        }

        public bool IsValid
        {
            get
            {
                return ctrlBlock != IntPtr.Zero;
            }
        }

        public void IncRef()
        {
            RefCountedPtrNativeBindings.RefCountedPtr_IncRef(ctrlBlock, ptr);
        }

        public void DecRef()
        {
            RefCountedPtrNativeBindings.RefCountedPtr_DecRef(ctrlBlock, ptr);
        }

        public T? GetValue()
        {
            HypClass? hypClass = HypClass.GetClass<T>();
            
            if (hypClass == null)
            {
                throw new Exception("Type " + typeof(T).Name + " does not have a registered HypClass");
            }

            HypDataBuffer hypDataBuffer;
            RefCountedPtrNativeBindings.RefCountedPtr_Get(ctrlBlock, ptr, out hypDataBuffer);

            T? value = (T?)hypDataBuffer.GetValue();

            hypDataBuffer.Destruct();

            return value;
        }
    }

    [StructLayout(LayoutKind.Sequential, Size = 16)]
    public struct WeakRefCountedPtr
    {
        private IntPtr ctrlBlock;
        private IntPtr ptr;

        public WeakRefCountedPtr(IntPtr ctrlBlock, IntPtr ptr)
        {
            this.ctrlBlock = ctrlBlock;
            this.ptr = ptr;
        }

        public bool IsValid
        {
            get
            {
                return ctrlBlock != IntPtr.Zero;
            }
        }

        public void IncRef()
        {
            RefCountedPtrNativeBindings.WeakRefCountedPtr_IncRef(ctrlBlock, ptr);
        }

        public void DecRef()
        {
            RefCountedPtrNativeBindings.WeakRefCountedPtr_DecRef(ctrlBlock, ptr);
        }

        public RefCountedPtr Lock()
        {
            uint refCount = RefCountedPtrNativeBindings.WeakRefCountedPtr_Lock(ctrlBlock, ptr);

            if (refCount == 0)
            {
                return RefCountedPtr.Null;
            }

            return new RefCountedPtr(ctrlBlock, ptr);
        }
    }

    [StructLayout(LayoutKind.Sequential, Size = 16)]
    public struct WeakRefCountedPtr<T>
    {
        private IntPtr ctrlBlock;
        private IntPtr ptr;

        public WeakRefCountedPtr()
        {
            this.ctrlBlock = IntPtr.Zero;
            this.ptr = IntPtr.Zero;
        }

        public WeakRefCountedPtr(IntPtr ctrlBlock, IntPtr ptr)
        {
            this.ctrlBlock = ctrlBlock;
            this.ptr = ptr;
        }

        public bool IsValid
        {
            get
            {
                return ctrlBlock != IntPtr.Zero;
            }
        }

        public void IncRef()
        {
            RefCountedPtrNativeBindings.WeakRefCountedPtr_IncRef(ctrlBlock, ptr);
        }

        public void DecRef()
        {
            RefCountedPtrNativeBindings.WeakRefCountedPtr_DecRef(ctrlBlock, ptr);
        }

        public RefCountedPtr<T> Lock()
        {
            uint refCount = RefCountedPtrNativeBindings.WeakRefCountedPtr_Lock(ctrlBlock, ptr);

            if (refCount == 0)
            {
                return RefCountedPtr<T>.Null;
            }

            return new RefCountedPtr<T>(ctrlBlock, ptr);
        }
    }
}
