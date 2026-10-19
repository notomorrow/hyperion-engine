using System;
using System.Runtime.InteropServices;
using System.Text;

namespace Hyperion
{
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct StrataString
    {
        public byte* Data;
        public uint Length;
        public uint Capacity;
    }

    public static unsafe class NativeBindings
    {
        private static class ClassCache<T>
        {
            public static readonly IntPtr ClassPtr = Class.GetClass(typeof(T)).Address;
        }

        public static T? GetManagedObject<T>(IntPtr objectPtr) where T : class
        {
            if (objectPtr == IntPtr.Zero)
            {
                return null;
            }

            ObjectReference objectReference;

            if (NativeBindings_GetManagedObject(ClassCache<T>.ClassPtr, objectPtr, &objectReference) == 0)
            {
                return null;
            }

            return objectReference.LoadObject() as T;
        }

        public static string TakeString(StrataString* value)
        {
            if (value->Data == null)
            {
                return string.Empty;
            }

            string result = Encoding.UTF8.GetString(value->Data, (int)value->Length);

            NativeBindings_FreeString(value->Data);
            value->Data = null;

            return result;
        }

        [DllImport("hyperion", EntryPoint = "NativeBindings_GetManagedObject")]
        private static extern byte NativeBindings_GetManagedObject(IntPtr classPtr, IntPtr objectPtr, ObjectReference* outObjectReference);

        [DllImport("hyperion", EntryPoint = "NativeBindings_FreeString")]
        private static extern void NativeBindings_FreeString(byte* data);
    }
}
