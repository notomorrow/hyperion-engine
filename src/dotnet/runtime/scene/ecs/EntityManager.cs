using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;

namespace Hyperion
{
    using ComponentID = uint;

    [HypClassBinding(Name="EntityManager")]
    public class EntityManager : HypObject
    {
        public EntityManager()
        {
        }

        public bool HasComponent<T>(Entity entity) where T : struct, IComponent
        {
            HypClass componentHypClass = HypClass.GetClass(typeof(T));
            
            return EntityManager_HasComponent(NativeAddress, componentHypClass.TypeID, entity.ID);
        }

        public void AddComponent<T>(Entity entity, T component) where T : struct, IComponent
        {
            HypClass componentHypClass = HypClass.GetClass(typeof(T));

            if (componentHypClass.Size != Marshal.SizeOf(component))
            {
                throw new Exception("Component size mismatch: " + componentHypClass.Size + " != " + Marshal.SizeOf(component));
            }

            AddComponent<T>(entity, componentHypClass, ref component);
        }

        private unsafe void AddComponent<T>(Entity entity, HypClass componentHypClass, ref T component) where T : struct, IComponent
        {
            fixed (void* ptr = &component)
            {
                EntityManager_AddComponent(NativeAddress, entity.ID, componentHypClass.TypeID, new IntPtr(ptr));
            }
        }

        public ref T GetComponent<T>(Entity entity) where T : struct, IComponent
        {
            HypClass componentHypClass = HypClass.GetClass(typeof(T));

            IntPtr componentPtr = EntityManager_GetComponent(NativeAddress, componentHypClass.TypeID, entity.ID);

            if (componentPtr == IntPtr.Zero)
            {
                throw new Exception("Failed to get component of type " + typeof(T).Name + " for entity " + entity.ID);
            }

            // marshal IntPtr to struct ref
            unsafe
            {
                return ref System.Runtime.CompilerServices.Unsafe.AsRef<T>(componentPtr.ToPointer());
            }
        }

        [DllImport("hyperion", EntryPoint = "EntityManager_HasComponent")]
        private static extern bool EntityManager_HasComponent(IntPtr entityManagerPtr, TypeID componentTypeId, IDBase entity);

        [DllImport("hyperion", EntryPoint = "EntityManager_GetComponent")]
        private static extern IntPtr EntityManager_GetComponent(IntPtr entityManagerPtr, TypeID componentTypeId, IDBase entity);

        [DllImport("hyperion", EntryPoint = "EntityManager_AddComponent")]
        private static extern void EntityManager_AddComponent(IntPtr entityManagerPtr, IDBase entity, TypeID componentTypeId, IntPtr componentPtr);
    }
}
