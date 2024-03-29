using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public delegate ManagedObject NewObjectDelegate();
    public delegate void FreeObjectDelegate(ManagedObject obj);

    [StructLayout(LayoutKind.Sequential)]
    public struct ManagedClass
    {
        private int typeHash;
        private IntPtr classObjectPtr;

        public void AddMethod(string methodName, Guid guid, string[] attributeNames)
        {
            IntPtr[] attributeNamesPtrs = new IntPtr[attributeNames.Length];

            for (int i = 0; i < attributeNames.Length; i++)
            {
                attributeNamesPtrs[i] = Marshal.StringToHGlobalAnsi(attributeNames[i]);
            }

            ManagedClass_AddMethod(this, methodName, guid, (uint)attributeNames.Length, attributeNamesPtrs);

            for (int i = 0; i < attributeNames.Length; i++)
            {
                Marshal.FreeHGlobal(attributeNamesPtrs[i]);
            }
        }

        public NewObjectDelegate NewObjectFunction
        {
            set
            {
                ManagedClass_SetNewObjectFunction(this, Marshal.GetFunctionPointerForDelegate(value));
            }
        }

        public FreeObjectDelegate FreeObjectFunction
        {
            set
            {
                ManagedClass_SetFreeObjectFunction(this, Marshal.GetFunctionPointerForDelegate(value));
            }
        }

        // Add a function pointer to the managed class
        [DllImport("libhyperion", EntryPoint = "ManagedClass_AddMethod")]
        private static extern void ManagedClass_AddMethod(ManagedClass managedClass, string methodName, Guid guid, uint numAttributes, IntPtr[] attributeNames);

        [DllImport("libhyperion", EntryPoint = "ManagedClass_SetNewObjectFunction")]
        private static extern ManagedClass ManagedClass_SetNewObjectFunction(ManagedClass managedClass, IntPtr newObjectFunctionPtr);

        [DllImport("libhyperion", EntryPoint = "ManagedClass_SetFreeObjectFunction")]
        private static extern ManagedClass ManagedClass_SetFreeObjectFunction(ManagedClass managedClass, IntPtr freeObjectFunctionPtr);
    }
}