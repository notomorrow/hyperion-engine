using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [StructLayout(LayoutKind.Sequential, Size = 4)]
    public struct HypMethodParameter
    {
        private TypeID typeId;

        public TypeID TypeID
        {
            get
            {
                return typeId;
            }
        }
    }

    [Flags]
    public enum HypMethodFlags : uint
    {
        None = 0x0,
        Static = 0x1,
        Member = 0x2
    }

    public struct HypMethod
    {
        public static readonly HypMethod Invalid = new HypMethod(IntPtr.Zero);

        internal IntPtr ptr;

        internal HypMethod(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        public Name Name
        {
            get
            {
                Name name;
                HypMethod_GetName(ptr, out name);
                return name;
            }
        }

        public TypeID ReturnTypeID
        {
            get
            {
                TypeID returnTypeId;
                HypMethod_GetReturnTypeID(ptr, out returnTypeId);
                return returnTypeId;
            }
        }

        public IEnumerable<HypMethodParameter> Parameters
        {
            get
            {
                IntPtr paramsPtr;
                uint count = HypMethod_GetParameters(ptr, out paramsPtr);

                for (int i = 0; i < count; i++)
                {
                    HypMethodParameter param = Marshal.PtrToStructure<HypMethodParameter>(paramsPtr + i * Marshal.SizeOf<HypMethodParameter>());
                    yield return param;
                }
            }
        }

        public HypMethodFlags Flags
        {
            get
            {
                return (HypMethodFlags)HypMethod_GetFlags(ptr);
            }
        }

        public bool IsStatic
        {
            get
            {
                return (Flags & HypMethodFlags.Static) != 0;
            }
        }

        public bool IsMemberFunction
        {
            get
            {
                return (Flags & HypMethodFlags.Member) != 0;
            }
        }

        public HypDataBuffer InvokeNativeWithThis(HypObject thisObject, object[]? args = null)
        {
            if (ptr == IntPtr.Zero)
            {
                throw new InvalidOperationException("Cannot invoke method: Invalid method");
            }

            uint numArgs = args == null ? 1 : (uint)args.Length + 1;

            bool isMemberFunction = IsMemberFunction;

            if (!IsMemberFunction)
            {
                throw new InvalidOperationException("Cannot invoke method: Method is not a member function");
            }

            IntPtr paramsPtr;
            uint numParams = HypMethod_GetParameters(ptr, out paramsPtr);

            if (numArgs != numParams)
            {
                throw new InvalidOperationException("Cannot invoke method: Invalid number of arguments - expected " + numParams + " but got " + numArgs);
            }

            bool shouldStackAlloc = numArgs * Marshal.SizeOf<HypDataBuffer>() < 1024;

            Span<HypDataBuffer> hypDataArgsBuffers = (shouldStackAlloc ? stackalloc HypDataBuffer[(int)numArgs] : new HypDataBuffer[(int)numArgs]);
            
            int argIndex = 0;

            HypDataBuffer.HypData_Construct(ref hypDataArgsBuffers[argIndex]);
            hypDataArgsBuffers[argIndex].SetValue(thisObject);
            argIndex++;

            if (numArgs > 1)
            {
                for (; argIndex < numArgs; argIndex++)
                {
                    HypDataBuffer.HypData_Construct(ref hypDataArgsBuffers[argIndex]);
                    hypDataArgsBuffers[argIndex].SetValue(args[argIndex - 1]);
                }
            }

            HypDataBuffer resultBuffer;

            // Args is pointer to contiguous HypDataBuffer objects
            unsafe
            {
                fixed (HypDataBuffer* argsPtr = hypDataArgsBuffers)
                {
                    bool result = HypMethod_Invoke(ptr, (IntPtr)argsPtr, numArgs, out resultBuffer);

                    for (int i = 0; i < numArgs; i++)
                        hypDataArgsBuffers[i].Dispose();

                    if (!result)
                        throw new InvalidOperationException("Failed to invoke method");
                }
            }

            return resultBuffer;
        }

        public HypDataBuffer InvokeNative(params object[] args)
        {
            if (ptr == IntPtr.Zero)
            {
                throw new InvalidOperationException("Cannot invoke method: Invalid method");
            }

            uint numArgs = (uint)args.Length;

            HypObject? thisObject = null;

            bool isMemberFunction = IsMemberFunction;

            if (isMemberFunction)
            {
                if (args.Length == 0)
                {
                    throw new InvalidOperationException("Cannot invoke method: Method is a member function but no thisObject was provided");
                }

                thisObject = args[0] as HypObject;

                if (thisObject == null)
                {
                    throw new InvalidOperationException("Cannot invoke method: Invalid thisObject");
                }
            }

            IntPtr paramsPtr;
            uint numParams = HypMethod_GetParameters(ptr, out paramsPtr);

            if (numArgs != numParams)
            {
                throw new InvalidOperationException("Cannot invoke method: Invalid number of arguments - expected " + numParams + " but got " + numArgs);
            }

            bool shouldStackAlloc = numArgs * Marshal.SizeOf<HypDataBuffer>() < 1024;

            Span<HypDataBuffer> hypDataArgsBuffers = numArgs > 0
                ? (shouldStackAlloc ? stackalloc HypDataBuffer[(int)numArgs] : new HypDataBuffer[(int)numArgs])
                : Span<HypDataBuffer>.Empty;
            
            if (numArgs > 0)
            {
                int argIndex = 0;

                if (thisObject != null)
                {
                    HypDataBuffer.HypData_Construct(ref hypDataArgsBuffers[argIndex]);
                    hypDataArgsBuffers[argIndex].SetValue(thisObject);
                    argIndex++;
                }

                for (; argIndex < numArgs; argIndex++)
                {
                    HypDataBuffer.HypData_Construct(ref hypDataArgsBuffers[argIndex]);
                    hypDataArgsBuffers[argIndex].SetValue(args[argIndex]);
                }
            }

            HypDataBuffer resultBuffer;

            // Args is pointer to contiguous HypDataBuffer objects
            unsafe
            {
                fixed (HypDataBuffer* argsPtr = hypDataArgsBuffers)
                {
                    bool result = HypMethod_Invoke(ptr, (IntPtr)argsPtr, numArgs, out resultBuffer);

                    for (int i = 0; i < numArgs; i++)
                        hypDataArgsBuffers[i].Dispose();

                    if (!result)
                        throw new InvalidOperationException("Failed to invoke method");
                }
            }

            return resultBuffer;
        }

        public HypData Invoke(params object[] args)
        {
            return HypData.FromBuffer(InvokeNative(args));
        }
        
        [DllImport("hyperion", EntryPoint = "HypMethod_GetName")]
        private static extern void HypMethod_GetName([In] IntPtr methodPtr, [Out] out Name name);

        [DllImport("hyperion", EntryPoint = "HypMethod_GetReturnTypeID")]
        private static extern void HypMethod_GetReturnTypeID([In] IntPtr methodPtr, [Out] out TypeID returnTypeId);

        [DllImport("hyperion", EntryPoint = "HypMethod_GetParameters")]
        private static extern uint HypMethod_GetParameters([In] IntPtr methodPtr, [Out] out IntPtr outParamsPtr);

        [DllImport("hyperion", EntryPoint = "HypMethod_GetFlags")]
        private static extern uint HypMethod_GetFlags([In] IntPtr methodPtr);

        [DllImport("hyperion", EntryPoint = "HypMethod_Invoke")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern unsafe bool HypMethod_Invoke([In] IntPtr methodPtr, [In] IntPtr argsPtr, uint numArgs, [Out] out HypDataBuffer outResult);
    }
}