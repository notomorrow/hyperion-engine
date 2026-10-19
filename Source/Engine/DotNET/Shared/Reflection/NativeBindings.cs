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

    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct StrataArray
    {
        public void* Data;
        public uint Length;
        public uint Capacity;

        public StrataArray(void* data, int length)
        {
            Data = data;
            Length = (uint)length;
            Capacity = (uint)length;
        }
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

        public static T? TakeManagedObject<T>(IntPtr objectPtr) where T : class
        {
            if (objectPtr == IntPtr.Zero)
            {
                return null;
            }

            try
            {
                return GetManagedObject<T>(objectPtr);
            }
            finally
            {
                NativeBindings_ReleaseObject(objectPtr);
            }
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

        public static T[] TakeArray<T>(StrataArray* value) where T : unmanaged
        {
            if (value->Data == null)
            {
                return Array.Empty<T>();
            }

            T[] result = new ReadOnlySpan<T>(value->Data, (int)value->Length).ToArray();

            NativeBindings_FreeString(value->Data);
            value->Data = null;

            return result;
        }

        public static T?[] TakeObjectArray<T>(StrataArray* value) where T : class
        {
            if (value->Data == null)
            {
                return Array.Empty<T?>();
            }

            IntPtr* objects = (IntPtr*)value->Data;
            T?[] result = new T?[value->Length];

            for (int i = 0; i < result.Length; i++)
            {
                result[i] = TakeManagedObject<T>(objects[i]);
            }

            NativeBindings_FreeString(value->Data);
            value->Data = null;

            return result;
        }

        public static string[] TakeStringArray(StrataArray* value)
        {
            if (value->Data == null)
            {
                return Array.Empty<string>();
            }

            StrataString* strings = (StrataString*)value->Data;
            string[] result = new string[value->Length];

            for (int i = 0; i < result.Length; i++)
            {
                result[i] = TakeString(&strings[i]);
            }

            NativeBindings_FreeString(value->Data);
            value->Data = null;

            return result;
        }

        public static T[] ToTypedArray<T>(Array? values) where T : unmanaged
        {
            if (values == null)
            {
                return Array.Empty<T>();
            }

            if (values is T[] typed)
            {
                return typed;
            }

            T[] result = new T[values.Length];
            int index = 0;

            foreach (object? value in values)
            {
                result[index++] = (T)value!;
            }

            return result;
        }

        public static IntPtr[] ToNativeAddresses(Array? values)
        {
            if (values == null)
            {
                return Array.Empty<IntPtr>();
            }

            IntPtr[] result = new IntPtr[values.Length];
            int index = 0;

            foreach (object? value in values)
            {
                result[index++] = value is ObjectBase obj ? obj.NativeAddress : IntPtr.Zero;
            }

            return result;
        }

        [DllImport("hyperion", EntryPoint = "NativeBindings_GetManagedObject")]
        private static extern byte NativeBindings_GetManagedObject(IntPtr classPtr, IntPtr objectPtr, ObjectReference* outObjectReference);

        [DllImport("hyperion", EntryPoint = "NativeBindings_ReleaseObject")]
        private static extern void NativeBindings_ReleaseObject(IntPtr objectPtr);

        [DllImport("hyperion", EntryPoint = "NativeBindings_FreeString")]
        private static extern void NativeBindings_FreeString(void* data);
    }
}
