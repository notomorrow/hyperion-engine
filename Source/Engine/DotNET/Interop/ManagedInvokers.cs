using System;
using System.Diagnostics;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Hyperion
{
    internal abstract unsafe class ReflectionInvoker
    {
        public abstract string Name { get; }

        public abstract void Invoke(object? thisObject, BoxedValueInternal** args, BoxedValueInternal* ret);
    }

    internal sealed unsafe class ReflectionMethodInvoker : ReflectionInvoker
    {
        private readonly MethodInfo _methodInfo;

        public ReflectionMethodInvoker(MethodInfo methodInfo)
        {
            _methodInfo = methodInfo;
        }

        public override string Name => _methodInfo.DeclaringType?.Name + "." + _methodInfo.Name;

        public override void Invoke(object? thisObject, BoxedValueInternal** args, BoxedValueInternal* ret)
        {
            if (!_methodInfo.IsStatic && thisObject == null)
            {
                throw new InvalidOperationException("Failed to get object reference for method: " + Name);
            }

            object?[] parameters = GetParameters((IntPtr)args);

            object? returnValue = _methodInfo.Invoke(thisObject, BindingFlags.DoNotWrapExceptions, null, parameters, null);

            if (_methodInfo.ReturnType != typeof(void) && ret != null)
            {
                ret->SetValue(returnValue);
            }
        }

        private object?[] GetParameters(IntPtr argsBoxedPtr)
        {
            ParameterInfo[] parameterInfos = _methodInfo.GetParameters();
            int numParams = parameterInfos.Length;

            if (numParams == 0)
            {
                return Array.Empty<object?>();
            }

            object?[] parameters = new object?[numParams];

            BoxedValueInternal* paramPtr = *(BoxedValueInternal**)argsBoxedPtr;
            int paramsOffset = 0;

            for (int paramIndex = 0; paramIndex < numParams; paramIndex++)
            {
                ParameterInfo parameterInfo = parameterInfos[paramIndex];

                Type parameterType = parameterInfo.ParameterType;

                if (parameterInfo.GetCustomAttribute(typeof(ParamArrayAttribute)) != null)
                {
                    int paramArraySize = 0;

                    for (IntPtr currentParamPtr = argsBoxedPtr + paramsOffset; (IntPtr)(*((BoxedValueInternal**)currentParamPtr)) != IntPtr.Zero; currentParamPtr += sizeof(IntPtr))
                    {
                        paramArraySize++;
                    }

                    Array paramArray = Array.CreateInstance(parameterType.GetElementType()!, paramArraySize);

                    int paramElementIndex = 0;

                    while ((IntPtr)paramPtr != IntPtr.Zero)
                    {
                        object? paramValue;

                        try
                        {
                            paramValue = paramPtr->GetValue();
                        }
                        catch (Exception ex)
                        {
                            throw new Exception("Failed to get params element value at index: " + paramElementIndex + " for method: " + Name, ex);
                        }

                        paramArray.SetValue(paramValue, paramElementIndex);

                        paramsOffset += sizeof(IntPtr);
                        paramPtr = *(BoxedValueInternal**)(argsBoxedPtr + paramsOffset);

                        paramElementIndex++;
                    }

                    parameters[paramIndex] = paramArray;

                    break;
                }

                try
                {
                    parameters[paramIndex] = paramPtr->GetValue();
                }
                catch (Exception ex)
                {
                    throw new Exception("Failed to get parameter value at index: " + paramIndex + " for method: " + Name, ex);
                }

                paramsOffset += sizeof(IntPtr);
                paramPtr = *(BoxedValueInternal**)(argsBoxedPtr + paramsOffset);
            }

            return parameters;
        }
    }

    internal sealed unsafe class ReflectionGetterInvoker : ReflectionInvoker
    {
        private readonly PropertyInfo _propertyInfo;

        public ReflectionGetterInvoker(PropertyInfo propertyInfo)
        {
            _propertyInfo = propertyInfo;
        }

        public override string Name => _propertyInfo.DeclaringType?.Name + "." + _propertyInfo.Name;

        public override void Invoke(object? thisObject, BoxedValueInternal** args, BoxedValueInternal* ret)
        {
            object? value = _propertyInfo.GetValue(thisObject, BindingFlags.DoNotWrapExceptions, null, null, null);

            if (ret != null)
            {
                ret->SetValue(value);
            }
        }
    }

    internal sealed unsafe class ReflectionSetterInvoker : ReflectionInvoker
    {
        private readonly PropertyInfo _propertyInfo;

        public ReflectionSetterInvoker(PropertyInfo propertyInfo)
        {
            _propertyInfo = propertyInfo;
        }

        public override string Name => _propertyInfo.DeclaringType?.Name + "." + _propertyInfo.Name;

        public override void Invoke(object? thisObject, BoxedValueInternal** args, BoxedValueInternal* ret)
        {
            object? value = (*args)->GetValue();

            _propertyInfo.SetValue(thisObject, value, BindingFlags.DoNotWrapExceptions, null, null, null);
        }
    }

    internal sealed class ReflectionObjectFactory
    {
        private const string ClassPtrFieldName = "_classPtr";
        private const string NativeAddressFieldName = "_nativeAddress";

        private readonly Type _type;

        public ReflectionObjectFactory(Type type)
        {
            _type = type;
        }

        public Type Type => _type;

        public object Create(IntPtr pClass, IntPtr nativeAddress)
        {
            object obj = RuntimeHelpers.GetUninitializedObject(_type);

            FieldInfo? classPtrField = null;
            FieldInfo? nativeAddressField = null;

            if (pClass != IntPtr.Zero)
            {
                if (nativeAddress == IntPtr.Zero)
                    throw new ArgumentNullException(nameof(nativeAddress));

                classPtrField = _type.GetField(ClassPtrFieldName, BindingFlags.Instance | BindingFlags.NonPublic | BindingFlags.Public | BindingFlags.FlattenHierarchy);
                nativeAddressField = _type.GetField(NativeAddressFieldName, BindingFlags.Instance | BindingFlags.NonPublic | BindingFlags.Public | BindingFlags.FlattenHierarchy);

                if (classPtrField == null || nativeAddressField == null)
                    throw new InvalidOperationException("Could not find classPtr or nativeAddress field on class " + _type.Name);

                classPtrField.SetValue(obj, pClass);
                nativeAddressField.SetValue(obj, nativeAddress);
            }

            ConstructorInfo? constructorInfo = _type.GetConstructor(BindingFlags.Public | BindingFlags.Instance, null, Type.EmptyTypes, null);

            if (constructorInfo == null)
                throw new InvalidOperationException("Failed to find empty constructor for type: " + _type.Name);

            try
            {
                constructorInfo.Invoke(obj, BindingFlags.DoNotWrapExceptions, null, null, null);
            }
            catch
            {
                // Derived field initializers run before ObjectBase takes its native ref, so the finalizer must not release one
                if (classPtrField != null && nativeAddressField != null)
                {
                    classPtrField.SetValue(obj, IntPtr.Zero);
                    nativeAddressField.SetValue(obj, IntPtr.Zero);
                }

                throw;
            }

            return obj;
        }

        public object Marshal(IntPtr ptr)
        {
            // Cannot create boxed byref-like values
            if (_type.IsByRefLike)
            {
                throw new InvalidOperationException($"Cannot marshal byref-like type {_type.Name} from pointer.");
            }

            object? obj = System.Runtime.InteropServices.Marshal.PtrToStructure(ptr, _type);
            Debug.Assert(obj != null, "Failed to marshal object from pointer");

            return obj!;
        }
    }

    internal static unsafe class ManagedInvokers
    {
        public static IntPtr InvokeGeneratedFptr => (IntPtr)(delegate* unmanaged<IntPtr, ObjectReference*, BoxedValueInternal**, BoxedValueInternal*, byte>)&InvokeGenerated;
        public static IntPtr InvokeReflectedFptr => (IntPtr)(delegate* unmanaged<IntPtr, ObjectReference*, BoxedValueInternal**, BoxedValueInternal*, byte>)&InvokeReflected;
        public static IntPtr NewObjectGeneratedFptr => (IntPtr)(delegate* unmanaged<IntPtr, byte, IntPtr, IntPtr, IntPtr, IntPtr, ObjectReference>)&NewObjectGenerated;
        public static IntPtr NewObjectReflectedFptr => (IntPtr)(delegate* unmanaged<IntPtr, byte, IntPtr, IntPtr, IntPtr, IntPtr, ObjectReference>)&NewObjectReflected;
        public static IntPtr MarshalObjectGeneratedFptr => (IntPtr)(delegate* unmanaged<IntPtr, IntPtr, uint, ObjectReference>)&MarshalObjectGenerated;
        public static IntPtr MarshalObjectReflectedFptr => (IntPtr)(delegate* unmanaged<IntPtr, IntPtr, uint, ObjectReference>)&MarshalObjectReflected;

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
        private static byte InvokeReflected(IntPtr context, ObjectReference* thisObjectReference, BoxedValueInternal** args, BoxedValueInternal* ret)
        {
            ReflectionInvoker? invoker = null;

            try
            {
                invoker = (ReflectionInvoker)GCHandle.FromIntPtr(context).Target!;
                invoker.Invoke(LoadThis(thisObjectReference), args, ret);

                return 1;
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Error, "Exception thrown invoking {0}: {1}", invoker?.Name, ex);

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

        [UnmanagedCallersOnly]
        private static ObjectReference NewObjectReflected(IntPtr context, byte keepAlive, IntPtr pClass, IntPtr nativeAddress, IntPtr pCtx, IntPtr pCallback)
        {
            ReflectionObjectFactory? factory = null;
            object obj;

            try
            {
                factory = (ReflectionObjectFactory)GCHandle.FromIntPtr(context).Target!;
                obj = factory.Create(pClass, nativeAddress);
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Error, "Exception thrown constructing object of type {0}: {1}", factory?.Type.Name, ex);

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

        [UnmanagedCallersOnly]
        private static ObjectReference MarshalObjectReflected(IntPtr context, IntPtr ptr, uint size)
        {
            ReflectionObjectFactory? factory = null;

            try
            {
                if (ptr == IntPtr.Zero)
                    throw new ArgumentNullException(nameof(ptr));

                factory = (ReflectionObjectFactory)GCHandle.FromIntPtr(context).Target!;

                return MakeMarshalledObjectReference(factory.Marshal(ptr));
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Error, "Exception thrown marshalling object of type {0}: {1}", factory?.Type.Name, ex);

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
