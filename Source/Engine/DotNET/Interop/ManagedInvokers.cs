using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    internal static unsafe class ManagedInvokers
    {
        public static IntPtr InvokeGeneratedFptr => (IntPtr)(delegate* unmanaged<IntPtr, ObjectReference*, BoxedValueInternal**, BoxedValueInternal*, byte>)&InvokeGenerated;
        public static IntPtr NewObjectGeneratedFptr => (IntPtr)(delegate* unmanaged<IntPtr, byte, IntPtr, IntPtr, IntPtr, IntPtr, ObjectReference>)&NewObjectGenerated;
        public static IntPtr MarshalObjectGeneratedFptr => (IntPtr)(delegate* unmanaged<IntPtr, IntPtr, uint, ObjectReference>)&MarshalObjectGenerated;

        private static object? LoadThis(ObjectReference* thisObjectReference)
        {
            return thisObjectReference == null ? null : thisObjectReference->LoadObject();
        }

        [UnmanagedCallersOnly]
        private static byte InvokeGenerated(IntPtr context, ObjectReference* thisObjectReference, BoxedValueInternal** args, BoxedValueInternal* ret)
        {
            try
            {
                ((delegate*<object?, BoxedValueInternal**, BoxedValueInternal*, void>)context)(LoadThis(thisObjectReference), args, ret);

                return 1;
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Error, "Exception thrown invoking managed method: {0}", ex);

                return 0;
            }
        }

        [UnmanagedCallersOnly]
        private static ObjectReference NewObjectGenerated(IntPtr context, byte keepAlive, IntPtr pClass, IntPtr nativeAddress, IntPtr pCtx, IntPtr pCallback)
        {
            object obj;

            try
            {
                obj = ((delegate*<IntPtr, IntPtr, object>)context)(pClass, nativeAddress);
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Error, "Exception thrown constructing managed object: {0}", ex);

                return default;
            }

            return CompleteNewObject(obj, keepAlive != 0, pCtx, pCallback);
        }

        private static ObjectReference CompleteNewObject(object obj, bool keepAlive, IntPtr pCtx, IntPtr pCallback)
        {
            GCHandle? gcHandleWeak = null;
            GCHandle? gcHandleStrong = null;

            try
            {
                gcHandleWeak = GCHandle.Alloc(obj, GCHandleType.Weak);

                if (pCallback != IntPtr.Zero)
                {
                    if (!obj.GetType().IsValueType)
                        throw new InvalidOperationException("InitializeObjectCallback can only be used with value types");

                    gcHandleStrong = GCHandle.Alloc(obj, GCHandleType.Pinned);

                    ((delegate* unmanaged<IntPtr, IntPtr, uint, void>)pCallback)(pCtx, ((GCHandle)gcHandleStrong).AddrOfPinnedObject(), (uint)HypMarshal.SizeOf(obj.GetType()));

                    if (!keepAlive)
                    {
                        ((GCHandle)gcHandleStrong).Free();
                        gcHandleStrong = null;
                    }
                }
                else if (keepAlive)
                {
                    gcHandleStrong = GCHandle.Alloc(obj, GCHandleType.Normal);
                }

                return new ObjectReference
                {
                    WeakHandle = GCHandle.ToIntPtr((GCHandle)gcHandleWeak),
                    StrongHandle = gcHandleStrong.HasValue ? GCHandle.ToIntPtr((GCHandle)gcHandleStrong) : IntPtr.Zero
                };
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Error, "Exception thrown constructing object of type {0}: {1}", obj.GetType().Name, ex);

                if (gcHandleStrong.HasValue)
                    ((GCHandle)gcHandleStrong).Free();

                if (gcHandleWeak.HasValue)
                    ((GCHandle)gcHandleWeak).Free();

                if (obj is ObjectBase objectBase)
                {
                    objectBase._classPtr = IntPtr.Zero;
                    objectBase._nativeAddress = IntPtr.Zero;
                }

                return default;
            }
        }

        [UnmanagedCallersOnly]
        private static ObjectReference MarshalObjectGenerated(IntPtr context, IntPtr ptr, uint size)
        {
            try
            {
                if (ptr == IntPtr.Zero)
                    throw new ArgumentNullException(nameof(ptr));

                return MakeMarshalledObjectReference(((delegate*<IntPtr, object>)context)(ptr));
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Error, "Exception thrown marshalling object: {0}", ex);

                return default;
            }
        }

        // Freshly boxed value has no other referrer, so it must be held by a strong handle until the caller reads it back
        private static ObjectReference MakeMarshalledObjectReference(object obj)
        {
            return new ObjectReference
            {
                WeakHandle = GCHandle.ToIntPtr(GCHandle.Alloc(obj, GCHandleType.Normal)),
                StrongHandle = IntPtr.Zero
            };
        }
    }
}
