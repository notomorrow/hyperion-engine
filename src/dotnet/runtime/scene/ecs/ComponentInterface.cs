using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public delegate void ComponentPropertyGetter(IntPtr componentPtr, IntPtr outDataPtr);

    public enum ComponentPropertyFlags : uint
    {
        None = 0x0,
        Read = 0x1,
        Write = 0x2,
        ReadWrite = Read | Write
    }

    [StructLayout(LayoutKind.Explicit, Size = 32)]
    public struct ComponentProperty
    {
        [FieldOffset(0)]
        Name name;

        [FieldOffset(8)]
        uint flags;

        // getter function pointer
        [FieldOffset(12)]
        IntPtr getter;

        // setter function pointer
        [FieldOffset(16)]
        IntPtr setter;

        public Name Name
        {
            get
            {
                return name;
            }
        }

        public bool IsReadable
        {
            get
            {
                return (flags & (uint)ComponentPropertyFlags.Read) != 0;
            }
        }

        public bool IsWritable
        {
            get
            {
                return (flags & (uint)ComponentPropertyFlags.Write) != 0;
            }
        }

        public bool IsReadOnly
        {
            get
            {
                return (flags & (uint)ComponentPropertyFlags.ReadWrite) == (uint)ComponentPropertyFlags.Read;
            }
        }

        public FBOMData GetValue(IntPtr componentPtr)
        {
            if (!IsReadable)
            {
                throw new Exception("Property is not readable");
            }

            FBOMData data = new FBOMData();

            if (!ComponentProperty_InvokeGetter(ref this, componentPtr, data.ptr))
            {
                throw new Exception("Failed to invoke property getter");
            }

            return data;
        }

        public FBOMData GetValue<T>(ref T component) where T : struct, IComponent
        {
            GCHandle componentHandle = GCHandle.Alloc(component, GCHandleType.Pinned);
            IntPtr componentPtr = componentHandle.AddrOfPinnedObject();

            FBOMData result = GetValue(componentPtr);

            componentHandle.Free();

            return result;
        }

        public void SetValue(IntPtr componentPtr, FBOMData data)
        {
            if (!IsWritable)
            {
                throw new Exception("Property is not writable");
            }

            if (!ComponentProperty_InvokeSetter(ref this, componentPtr, data.ptr))
            {
                throw new Exception("Failed to invoke property setter");
            }
        }

        public void SetValue<T>(ref T component, FBOMData data) where T : struct, IComponent
        {
            GCHandle componentHandle = GCHandle.Alloc(component, GCHandleType.Pinned);
            IntPtr componentPtr = componentHandle.AddrOfPinnedObject();

            SetValue(componentPtr, data);

            componentHandle.Free();
        }

        [DllImport("hyperion", EntryPoint = "ComponentProperty_InvokeGetter")]
        private static extern bool ComponentProperty_InvokeGetter([In] ref ComponentProperty property, IntPtr componentPtr, IntPtr outDataPtr);

        [DllImport("hyperion", EntryPoint = "ComponentProperty_InvokeSetter")]
        private static extern bool ComponentProperty_InvokeSetter([In] ref ComponentProperty property, IntPtr componentPtr, IntPtr dataPtr);
    }

    public class ComponentInterfaceBase
    {
        [DllImport("hyperion", EntryPoint = "ComponentInterface_GetProperty")]
        internal static extern bool ComponentInterface_GetProperty(IntPtr componentInterfacePtr, [MarshalAs(UnmanagedType.LPStr)] string key, [Out] out ComponentProperty property);
    }

    public class ComponentInterface<T> : ComponentInterfaceBase
    {
        internal IntPtr ptr;

        public ComponentInterface(IntPtr ptr) : base()
        {
            this.ptr = ptr;
        }

        public ComponentProperty this[string key]
        {
            get
            {
                ComponentProperty property;

                if (ComponentInterface_GetProperty(ptr, key, out property))
                {
                    return property;
                }

                throw new Exception("Property \"" + key + "\" not found");
            }
            set
            {
                // @TODO
            }
        }
    }
}