using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class FBOMType
    {

        public static readonly FBOMType Unset;
        public static readonly FBOMType UInt32;
        public static readonly FBOMType UInt64;
        public static readonly FBOMType Int32;
        public static readonly FBOMType Int64;
        public static readonly FBOMType Float;
        public static readonly FBOMType Bool;
        public static readonly FBOMType Byte;
        public static readonly FBOMType Name;
        public static readonly FBOMType Matrix3;
        public static readonly FBOMType Matrix4;
        public static readonly FBOMType Vec2f;
        public static readonly FBOMType Vec3f;
        public static readonly FBOMType Vec4f;
        public static readonly FBOMType Quaternion;

        static FBOMType()
        {
            Unset = new FBOMType(FBOMType_Unset());
            UInt32 = new FBOMType(FBOMType_UInt32());
            UInt64 = new FBOMType(FBOMType_UInt64());
            Int32 = new FBOMType(FBOMType_Int32());
            Int64 = new FBOMType(FBOMType_Int64());
            Float = new FBOMType(FBOMType_Float());
            Bool = new FBOMType(FBOMType_Bool());
            Byte = new FBOMType(FBOMType_Byte());
            Name = new FBOMType(FBOMType_Name());
            Matrix3 = new FBOMType(FBOMType_Matrix3());
            Matrix4 = new FBOMType(FBOMType_Matrix4());
            Vec2f = new FBOMType(FBOMType_Vec2f());
            Vec3f = new FBOMType(FBOMType_Vec3f());
            Vec4f = new FBOMType(FBOMType_Vec4f());
            Quaternion = new FBOMType(FBOMType_Quaternion());
        }

        internal IntPtr ptr;

        internal FBOMType(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        ~FBOMType()
        {
            FBOMType_Destroy(this.ptr);
        }

        public override bool Equals(object other)
        {
            if (other is FBOMType)
            {
                return FBOMType_Equals(this.ptr, ((FBOMType)other).ptr);
            }

            return false;
        }

        public bool IsOrExtends(FBOMType other, bool allowUnbounded = true)
        {
            return FBOMType_IsOrExtends(this.ptr, other.ptr, allowUnbounded);
        }

        [DllImport("hyperion", EntryPoint = "FBOMType_Unset")]
        public static extern IntPtr FBOMType_Unset();

        [DllImport("hyperion", EntryPoint = "FBOMType_UInt32")]
        public static extern IntPtr FBOMType_UInt32();

        [DllImport("hyperion", EntryPoint = "FBOMType_UInt64")]
        public static extern IntPtr FBOMType_UInt64();

        [DllImport("hyperion", EntryPoint = "FBOMType_Int32")]
        public static extern IntPtr FBOMType_Int32();

        [DllImport("hyperion", EntryPoint = "FBOMType_Int64")]
        public static extern IntPtr FBOMType_Int64();

        [DllImport("hyperion", EntryPoint = "FBOMType_Float")]
        public static extern IntPtr FBOMType_Float();

        [DllImport("hyperion", EntryPoint = "FBOMType_Bool")]
        public static extern IntPtr FBOMType_Bool();

        [DllImport("hyperion", EntryPoint = "FBOMType_Byte")]
        public static extern IntPtr FBOMType_Byte();

        [DllImport("hyperion", EntryPoint = "FBOMType_Name")]
        public static extern IntPtr FBOMType_Name();

        [DllImport("hyperion", EntryPoint = "FBOMType_Matrix3")]
        public static extern IntPtr FBOMType_Matrix3();

        [DllImport("hyperion", EntryPoint = "FBOMType_Matrix4")]
        public static extern IntPtr FBOMType_Matrix4();

        [DllImport("hyperion", EntryPoint = "FBOMType_Vec2f")]
        public static extern IntPtr FBOMType_Vec2f();

        [DllImport("hyperion", EntryPoint = "FBOMType_Vec3f")]
        public static extern IntPtr FBOMType_Vec3f();

        [DllImport("hyperion", EntryPoint = "FBOMType_Vec4f")]
        public static extern IntPtr FBOMType_Vec4f();

        [DllImport("hyperion", EntryPoint = "FBOMType_Quaternion")]
        public static extern IntPtr FBOMType_Quaternion();

        [DllImport("hyperion", EntryPoint = "FBOMType_Destroy")]
        private static extern void FBOMType_Destroy(IntPtr ptr);

        [DllImport("hyperion", EntryPoint = "FBOMType_Equals")]
        private static extern bool FBOMType_Equals(IntPtr lhs, IntPtr rhs);

        [DllImport("hyperion", EntryPoint = "FBOMType_IsOrExtends")]
        private static extern bool FBOMType_IsOrExtends(IntPtr lhs, IntPtr rhs, bool allowUnbounded);
    }
}