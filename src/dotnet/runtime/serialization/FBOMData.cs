using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class FBOMData : IDisposable
    {

        internal IntPtr ptr;
        private FBOMType type = FBOMType.Unset;

        public FBOMData()
        {
            this.ptr = FBOMData_Create(IntPtr.Zero);
            this.type = new FBOMType(FBOMData_GetType(this.ptr));
        }

        internal FBOMData(IntPtr ptr)
        {
            this.ptr = ptr;
            this.type = new FBOMType(FBOMData_GetType(this.ptr));
        }

        public void Dispose()
        {
            FBOMData_Destroy(this.ptr);
        }

        public FBOMType Type
        {
            get
            {
                return type;
            }
        }

        public ulong TotalSize
        {
            get
            {
                return FBOMData_TotalSize(this.ptr);
            }
        }

        public uint? GetUInt32()
        {
            uint value;

            if (FBOMData_GetUInt32(this.ptr, out value))
            {
                return value;
            }

            return null;
        }

        public ulong? GetUInt64()
        {
            ulong value;

            if (FBOMData_GetUInt64(this.ptr, out value))
            {
                return value;
            }

            return null;
        }

        public int? GetInt32()
        {
            int value;

            if (FBOMData_GetInt32(this.ptr, out value))
            {
                return value;
            }

            return null;
        }

        public long? GetInt64()
        {
            long value;

            if (FBOMData_GetInt64(this.ptr, out value))
            {
                return value;
            }

            return null;
        }

        public float? GetFloat()
        {
            float value;

            if (FBOMData_GetFloat(this.ptr, out value))
            {
                return value;
            }

            return null;
        }

        public bool? GetBool()
        {
            bool value;

            if (FBOMData_GetBool(this.ptr, out value))
            {
                return value;
            }

            return null;
        }

        public byte? GetByte()
        {
            byte value;

            if (FBOMData_GetByte(this.ptr, out value))
            {
                return value;
            }

            return null;
        }

        public Name? GetName()
        {
            Name value;

            if (FBOMData_GetName(this.ptr, out value))
            {
                return value;
            }

            return null;
        }

        // public Matrix3? GetMatrix3()

        public Matrix4? GetMatrix4()
        {
            Matrix4 value;

            if (FBOMData_GetMatrix4(this.ptr, out value))
            {
                return value;
            }

            return null;
        }

        public Vec2f? GetVec2f()
        {
            Vec2f value;

            if (FBOMData_GetVec2f(this.ptr, out value))
            {
                return value;
            }

            return null;
        }

        public Vec3f? GetVec3f()
        {
            Vec3f value;

            if (FBOMData_GetVec3f(this.ptr, out value))
            {
                return value;
            }

            return null;
        }

        // public Vec4f? GetVec4f()
        // {
        //     Vec4f value;

        //     if (FBOMData_GetVec4f(this.ptr, out value))
        //     {
        //         return value;
        //     }

        //     return null;
        // }

        public Quaternion? GetQuaternion()
        {
            Quaternion value;

            if (FBOMData_GetQuaternion(this.ptr, out value))
            {
                return value;
            }

            return null;
        }

        [DllImport("hyperion", EntryPoint = "FBOMData_Create")]
        private static extern IntPtr FBOMData_Create(IntPtr typePtr);

        [DllImport("hyperion", EntryPoint = "FBOMData_Destroy")]
        private static extern void FBOMData_Destroy(IntPtr dataPtr);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetType")]
        private static extern IntPtr FBOMData_GetType(IntPtr dataPtr);

        [DllImport("hyperion", EntryPoint = "FBOMData_TotalSize")]
        private static extern ulong FBOMData_TotalSize(IntPtr dataPtr);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetUInt32")]
        private static extern bool FBOMData_GetUInt32(IntPtr dataPtr, [Out] out uint value);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetUInt64")]
        private static extern bool FBOMData_GetUInt64(IntPtr dataPtr, [Out] out ulong value);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetInt32")]
        private static extern bool FBOMData_GetInt32(IntPtr dataPtr, [Out] out int value);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetInt64")]
        private static extern bool FBOMData_GetInt64(IntPtr dataPtr, [Out] out long value);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetFloat")]
        private static extern bool FBOMData_GetFloat(IntPtr dataPtr, [Out] out float value);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetBool")]
        private static extern bool FBOMData_GetBool(IntPtr dataPtr, [Out] out bool value);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetByte")]
        private static extern bool FBOMData_GetByte(IntPtr dataPtr, [Out] out byte value);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetName")]
        private static extern bool FBOMData_GetName(IntPtr dataPtr, [Out] out Name value);

        // [DllImport("hyperion", EntryPoint = "FBOMData_GetMatrix3")]
        // private static extern bool FBOMData_GetMatrix3(IntPtr dataPtr, [Out] out Matrix3 value);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetMatrix4")]
        private static extern bool FBOMData_GetMatrix4(IntPtr dataPtr, [Out] out Matrix4 value);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetVec2f")]
        private static extern bool FBOMData_GetVec2f(IntPtr dataPtr, [Out] out Vec2f value);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetVec3f")]
        private static extern bool FBOMData_GetVec3f(IntPtr dataPtr, [Out] out Vec3f value);

        // [DllImport("hyperion", EntryPoint = "FBOMData_GetVec4f")]
        // private static extern bool FBOMData_GetVec4f(IntPtr dataPtr, [Out] out Vec4f value);

        [DllImport("hyperion", EntryPoint = "FBOMData_GetQuaternion")]
        private static extern bool FBOMData_GetQuaternion(IntPtr dataPtr, [Out] out Quaternion value);
    }
}