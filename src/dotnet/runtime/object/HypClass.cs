using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [Flags]
    public enum HypClassFlags : uint
    {
        None = 0x0,
        ClassType = 0x1,
        StructType = 0x2
    }

    public struct HypClass
    {
        public static readonly HypClass Invalid = new HypClass(IntPtr.Zero);

        private IntPtr ptr;

        internal HypClass(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        internal IntPtr Address
        {
            get
            {
                return ptr;
            }
            set
            {
                ptr = value;
            }
        }

        public bool IsValid
        {
            get
            {
                return ptr != IntPtr.Zero;
            }
        }

        public Name Name
        {
            get
            {
                Name name = new Name(0);
                HypClass_GetName(ptr, out name);
                return name;
            }
        }

        public TypeID TypeID
        {
            get
            {
                TypeID typeId;
                HypClass_GetTypeID(ptr, out typeId);
                return typeId;
            }
        }

        public uint Size
        {
            get
            {
                return HypClass_GetSize(ptr);
            }
        }

        public HypClassFlags Flags
        {
            get
            {
                return (HypClassFlags)HypClass_GetFlags(ptr);
            }
        }

        public bool IsClassType
        {
            get
            {
                return (Flags & HypClassFlags.ClassType) != 0;
            }
        }

        public bool IsStructType
        {
            get
            {
                return (Flags & HypClassFlags.StructType) != 0;
            }
        }

        public IEnumerable<HypProperty> Properties
        {
            get
            {
                IntPtr propertiesPtr;
                uint count = HypClass_GetProperties(ptr, out propertiesPtr);

                for (int i = 0; i < count; i++)
                {
                    IntPtr propertyPtr = Marshal.ReadIntPtr(propertiesPtr, i * IntPtr.Size);
                    yield return new HypProperty(propertyPtr);
                }
            }
        }

        public HypProperty? GetProperty(Name name)
        {
            IntPtr propertyPtr = HypClass_GetProperty(ptr, ref name);

            if (propertyPtr == IntPtr.Zero)
            {
                return null;
            }

            return new HypProperty(propertyPtr);
        }

        public IEnumerable<HypMethod> Methods
        {
            get
            {
                IntPtr methodsPtr;
                uint count = HypClass_GetMethods(ptr, out methodsPtr);

                for (int i = 0; i < count; i++)
                {
                    IntPtr methodPtr = Marshal.ReadIntPtr(methodsPtr, i * IntPtr.Size);
                    yield return new HypMethod(methodPtr);
                }
            }
        }

        public HypMethod? GetMethod(Name name)
        {
            IntPtr methodPtr = HypClass_GetMethod(ptr, ref name);

            if (methodPtr == IntPtr.Zero)
            {
                return null;
            }

            return new HypMethod(methodPtr);
        }

        public IEnumerable<HypField> Fields
        {
            get
            {
                IntPtr fieldsPtr;
                uint count = HypClass_GetFields(ptr, out fieldsPtr);

                for (int i = 0; i < count; i++)
                {
                    IntPtr fieldPtr = Marshal.ReadIntPtr(fieldsPtr, i * IntPtr.Size);
                    yield return new HypField(fieldPtr);
                }
            }
        }

        public HypField? GetField(Name name)
        {
            IntPtr fieldPtr = HypClass_GetField(ptr, ref name);

            if (fieldPtr == IntPtr.Zero)
            {
                return null;
            }

            return new HypField(fieldPtr);
        }

        /// <summary>
        ///  Creates a native instance of this object. The object has an initial ref count of zero,
        ///  so the reference must be manually decremented to destroy the object (with HypObject_DecRef)  
        /// </summary>
        public IntPtr CreateInstance()
        {
            IntPtr instancePtr = HypClass_CreateInstance(ptr);

            if (instancePtr == IntPtr.Zero)
            {
                throw new Exception("Failed to initialize HypObject");
            }

            return instancePtr;
        }

        public IntPtr InitInstance(IntPtr classObjectPtr, ObjectReference objectReference)
        {
            IntPtr instancePtr = HypClass_InitInstance(ptr, classObjectPtr, ref objectReference);

            if (instancePtr == IntPtr.Zero)
            {
                throw new Exception("Failed to initialize HypObject");
            }

            return instancePtr;
        }

        public static bool operator==(HypClass a, HypClass b)
        {
            return a.ptr == b.ptr;
        }

        public static bool operator!=(HypClass a, HypClass b)
        {
            return a.ptr != b.ptr;
        }

        public static HypClass? GetClass(string name)
        {
            IntPtr ptr = HypClass_GetClassByName(name);

            if (ptr == IntPtr.Zero)
            {
                return null;
            }

            return new HypClass(ptr);
        }

        public static HypClass GetClass<T>()
        {
            return GetClass(typeof(T));
        }

        public static HypClass GetClass(Type type)
        {
            HypClassBinding? hypClassBindingAttribute = HypClassBinding.ForType(type);

            if (hypClassBindingAttribute == null)
            {
                throw new InvalidOperationException($"Type {type.Name} is not a HypObject");
            }

            return hypClassBindingAttribute.HypClass;
        }
        
        [DllImport("hyperion", EntryPoint = "HypClass_CreateInstance")]
        private static extern IntPtr HypClass_CreateInstance([In] IntPtr hypClassPtr);
        
        [DllImport("hyperion", EntryPoint = "HypClass_InitInstance")]
        private static extern IntPtr HypClass_InitInstance([In] IntPtr hypClassPtr, [In] IntPtr classObjectPtr, [In] ref ObjectReference objectReference);

        [DllImport("hyperion", EntryPoint = "HypClass_GetClassByName")]
        private static extern IntPtr HypClass_GetClassByName([MarshalAs(UnmanagedType.LPStr)] string name);
        
        [DllImport("hyperion", EntryPoint = "HypClass_GetName")]
        private static extern void HypClass_GetName([In] IntPtr hypClassPtr, [Out] out Name name);

        [DllImport("hyperion", EntryPoint = "HypClass_GetTypeID")]
        private static extern void HypClass_GetTypeID([In] IntPtr hypClassPtr, [Out] out TypeID typeId);
        
        [DllImport("hyperion", EntryPoint = "HypClass_GetSize")]
        private static extern uint HypClass_GetSize([In] IntPtr hypClassPtr);

        [DllImport("hyperion", EntryPoint = "HypClass_GetFlags")]
        private static extern uint HypClass_GetFlags([In] IntPtr hypClassPtr);

        [DllImport("hyperion", EntryPoint = "HypClass_GetProperties")]
        private static extern uint HypClass_GetProperties([In] IntPtr hypClassPtr, [Out] out IntPtr outPropertiesPtr);

        [DllImport("hyperion", EntryPoint = "HypClass_GetProperty")]
        private static extern IntPtr HypClass_GetProperty([In] IntPtr hypClassPtr, [In] ref Name name);

        [DllImport("hyperion", EntryPoint = "HypClass_GetMethods")]
        private static extern uint HypClass_GetMethods([In] IntPtr hypClassPtr, [Out] out IntPtr outMethodsPtr);

        [DllImport("hyperion", EntryPoint = "HypClass_GetMethod")]
        private static extern IntPtr HypClass_GetMethod([In] IntPtr hypClassPtr, [In] ref Name name);

        [DllImport("hyperion", EntryPoint = "HypClass_GetFields")]
        private static extern uint HypClass_GetFields([In] IntPtr hypClassPtr, [Out] out IntPtr outFieldsPtr);

        [DllImport("hyperion", EntryPoint = "HypClass_GetField")]
        private static extern IntPtr HypClass_GetField([In] IntPtr hypClassPtr, [In] ref Name name);
    }
}