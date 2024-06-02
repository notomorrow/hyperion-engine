using System;

namespace Hyperion
{
    public struct ComponentWrapper<T>
    {
        internal IntPtr componentPtr;
        internal IntPtr componentInterfacePtr;

        public FBOMData this[string key]
        {
            get
            {
                return GetProperty(key).GetValue(componentPtr);
            }
            set
            {
                GetProperty(key).SetValue(componentPtr, value);
            }
        }

        private ComponentProperty GetProperty(string key)
        {
            ComponentProperty property;

            if (!ComponentInterfaceBase.ComponentInterface_GetProperty(componentInterfacePtr, key, out property))
            {
                throw new Exception("Property \"" + key + "\" not found");
            }

            return property;
        }

        // public ComponentWrapper(IntPtr componentPtr, ComponentInterface<T> componentInterface)
        // {
        //     this.componentPtr = componentPtr;
        //     this.componentInterface = componentInterface;
        // }

        // private ComponentInterface<T> GetInterface()
        // {
        //     IntPtr componentInterfacePtr = ComponentRegistry_GetComponentInterface(typeId);

        //     if (componentInterfacePtr == IntPtr.Zero)
        //     {
        //         throw new Exception("Failed to get component interface for type " + typeof(T).Name);
        //     }

        //     return new ComponentInterface<T>(componentInterfacePtr);
        // }

        // [DllImport("hyperion", EntryPoint = "ComponentRegistry_GetComponentInterface")]
        // private static extern ComponentID ComponentRegistry_GetComponentInterface(TypeID typeId);
    }
}