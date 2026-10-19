using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;

namespace Hyperion
{
    internal struct StoredDelegate
    {
        public Guid AssemblyGuid { get; set; }
        public Guid Guid { get; set; }
        public GCHandle GCHandle { get; set; }
    }

    internal class DelegateCache : IBasicCache
    {
        private static readonly DelegateCache instance = new DelegateCache();

        public static DelegateCache Instance
        {
            get
            {
                return instance;
            }
        }

        private object lockObject = new object();
        private Dictionary<Guid, StoredDelegate> cache = new Dictionary<Guid, StoredDelegate>();

        public void Add(Guid assemblyGuid, Guid guid, GCHandle gcHandle)
        {
            lock (lockObject)
            {
                if (cache.ContainsKey(guid))
                    throw new Exception("Delegate already exists in cache");

                cache.Add(guid, new StoredDelegate
                {
                    AssemblyGuid = assemblyGuid,
                    Guid = guid,
                    GCHandle = gcHandle
                });
            }
        }

        public bool Remove(Guid guid)
        {
            lock (lockObject)
            {
                if (!cache.ContainsKey(guid))
                    return false;

                cache[guid].GCHandle.Free();
                cache.Remove(guid);
            }

            return true;
        }

        public int RemoveForAssembly(Guid assemblyGuid)
        {
            lock (lockObject)
            {
                List<Guid> keysToRemove = new List<Guid>();

                foreach (KeyValuePair<Guid, StoredDelegate> entry in cache)
                {
                    if (entry.Value.AssemblyGuid == assemblyGuid)
                    {
                        entry.Value.GCHandle.Free();

                        keysToRemove.Add(entry.Key);
                    }
                }

                foreach (Guid key in keysToRemove)
                {
                    cache.Remove(key);
                }

                return keysToRemove.Count;
            }
        }
    }

    [Flags]
    public enum ManagedClassFlags : uint
    {
        None = 0x0,
        ClassType = 0x1,
        StructType = 0x2,
        EnumType = 0x4,
        Abstract = 0x8
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct ManagedClassDesc
    {
        internal long typeHash;
        internal IntPtr pClass;
        internal Guid assemblyGuid;
        internal ManagedClassFlags flags;

        public void SetAttributes(ref ManagedAttributeHolder managedAttributeHolder)
        {
            unsafe
            {
                fixed (ManagedAttributeHolder* managedAttributeHolderPtr = &managedAttributeHolder)
                {
                    ManagedClass_SetAttributes(pClass, (IntPtr)managedAttributeHolderPtr);
                }
            }
        }

        public void AddMethod(string methodName, Guid guid, IntPtr invokeFptr, IntPtr invokeContext, ref ManagedAttributeHolder managedAttributeHolder)
        {
            IntPtr methodNamePtr = Marshal.StringToCoTaskMemUTF8(methodName);

            unsafe
            {
                fixed (ManagedAttributeHolder* managedAttributeHolderPtr = &managedAttributeHolder)
                {
                    ManagedClass_AddMethod(pClass, methodNamePtr, guid, invokeFptr, invokeContext, (IntPtr)managedAttributeHolderPtr);
                }
            }

            Marshal.FreeCoTaskMem(methodNamePtr);
        }

        public void AddProperty(string propertyName, Guid guid, IntPtr getterFptr, IntPtr getterContext, IntPtr setterFptr, IntPtr setterContext, ref ManagedAttributeHolder managedAttributeHolder)
        {
            IntPtr propertyNamePtr = Marshal.StringToCoTaskMemUTF8(propertyName);

            unsafe
            {
                fixed (ManagedAttributeHolder* managedAttributeHolderPtr = &managedAttributeHolder)
                {
                    ManagedClass_AddProperty(pClass, propertyNamePtr, guid, getterFptr, getterContext, setterFptr, setterContext, (IntPtr)managedAttributeHolderPtr);
                }
            }

            Marshal.FreeCoTaskMem(propertyNamePtr);
        }

        /// <summary>
        /// Pointer to the C++ dotnet::ManagedClass object
        /// </summary>
        public IntPtr ClassObjectPtr
        {
            get
            {
                return pClass;
            }
        }

        public void SetNewObjectFunction(IntPtr newObjectFptr, IntPtr newObjectContext)
        {
            ManagedClass_SetNewObjectFunction(pClass, newObjectFptr, newObjectContext);
        }

        public void SetMarshalObjectFunction(IntPtr marshalObjectFptr, IntPtr marshalObjectContext)
        {
            ManagedClass_SetMarshalObjectFunction(pClass, marshalObjectFptr, marshalObjectContext);
        }

        [DllImport("hyperion", EntryPoint = "ManagedClass_SetAttributes")]
        private static extern void ManagedClass_SetAttributes([In] IntPtr managedClass, IntPtr managedAttributeHolderPtr);

        [DllImport("hyperion", EntryPoint = "ManagedClass_AddMethod")]
        private static extern void ManagedClass_AddMethod([In] IntPtr managedClass, IntPtr methodNamePtr, Guid guid, IntPtr invokeFptr, IntPtr invokeContext, IntPtr managedAttributeHolderPtr);

        [DllImport("hyperion", EntryPoint = "ManagedClass_AddProperty")]
        private static extern void ManagedClass_AddProperty([In] IntPtr managedClass, IntPtr propertyNamePtr, Guid guid, IntPtr getterFptr, IntPtr getterContext, IntPtr setterFptr, IntPtr setterContext, IntPtr managedAttributeHolderPtr);

        [DllImport("hyperion", EntryPoint = "ManagedClass_SetNewObjectFunction")]
        private static extern void ManagedClass_SetNewObjectFunction([In] IntPtr managedClass, IntPtr newObjectFptr, IntPtr newObjectContext);

        [DllImport("hyperion", EntryPoint = "ManagedClass_SetMarshalObjectFunction")]
        private static extern void ManagedClass_SetMarshalObjectFunction([In] IntPtr managedClass, IntPtr marshalObjectFptr, IntPtr marshalObjectContext);
    }
}
