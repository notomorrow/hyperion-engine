using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [StructLayout(LayoutKind.Sequential, Size = 4)]
    public struct ManagedHandle
    {
        [MarshalAs(UnmanagedType.U4)]
        public uint id;

        public bool IsValid
        {
            get
            {
                return id != 0;
            }
        }

        public void IncRef(TypeID typeId)
        {
            ManagedHandle_IncRef(typeId, this);
        }

        public void DecRef(TypeID typeId)
        {
            ManagedHandle_DecRef(typeId, this);

            id = 0;
        }

        public uint GetRefCountStrong(TypeID typeId)
        {
            return ManagedHandle_GetRefCountStrong(typeId, this);
        }

        public uint GetRefCountWeak(TypeID typeId)
        {
            return ManagedHandle_GetRefCountWeak(typeId, this);
        }

        [DllImport("hyperion", EntryPoint = "ManagedHandle_IncRef")]
        private static extern void ManagedHandle_IncRef(TypeID type_id, ManagedHandle handle);

        [DllImport("hyperion", EntryPoint = "ManagedHandle_DecRef")]
        private static extern void ManagedHandle_DecRef(TypeID type_id, ManagedHandle handle);

        [DllImport("hyperion", EntryPoint = "ManagedHandle_GetRefCountStrong")]
        private static extern uint ManagedHandle_GetRefCountStrong(TypeID type_id, ManagedHandle handle);

        [DllImport("hyperion", EntryPoint = "ManagedHandle_GetRefCountWeak")]
        private static extern uint ManagedHandle_GetRefCountWeak(TypeID type_id, ManagedHandle handle);
    }
}