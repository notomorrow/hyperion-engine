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

        public FBOMData GetValue<T>(ref T component) where T : struct, IComponent
        {
            if (!IsReadable)
            {
                throw new Exception("Property is not readable");
            }

            FBOMData data = new FBOMData();

            GCHandle componentHandle = GCHandle.Alloc(component, GCHandleType.Pinned);
            IntPtr componentPtr = componentHandle.AddrOfPinnedObject();

            if (!ComponentProperty_InvokeGetter(ref this, componentPtr, data.ptr))
            {
                throw new Exception("Failed to invoke property getter");
            }

            componentHandle.Free();

            return data;
        }

        public void SetValue<T>(ref T component, FBOMData data) where T : struct, IComponent
        {
            if (!IsWritable)
            {
                throw new Exception("Property is not writable");
            }

            GCHandle componentHandle = GCHandle.Alloc(component, GCHandleType.Pinned);
            IntPtr componentPtr = componentHandle.AddrOfPinnedObject();

            if (!ComponentProperty_InvokeSetter(ref this, componentPtr, data.ptr))
            {
                throw new Exception("Failed to invoke property setter");
            }

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
        protected static extern bool ComponentInterface_GetProperty(IntPtr componentInterfacePtr, [MarshalAs(UnmanagedType.LPStr)] string key, [Out] out ComponentProperty property);
    }

    public class ComponentInterface<T> : ComponentInterfaceBase
    {
        private IntPtr componentInterfacePtr;

        public ComponentInterface(IntPtr componentInterfacePtr) : base()
        {
            this.componentInterfacePtr = componentInterfacePtr;
        }

        public ComponentProperty this[string key]
        {
            get
            {
                ComponentProperty property;

                if (ComponentInterface_GetProperty(componentInterfacePtr, key, out property))
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