using System;
using System.Runtime.InteropServices;
using System.Reflection;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

public delegate void InitializeAssemblyDelegate(IntPtr classHolderPtr, string assemblyPath);

namespace Hyperion
{
    public delegate void InvokeMethodDelegate(Guid managedMethodGuid, Guid thisObjectGuid, IntPtr paramsPtr, IntPtr outPtr);

    public class NativeInterop
    {
        public static InitializeAssemblyDelegate InitializeAssemblyDelegate = InitializeAssembly;
        public static InvokeMethodDelegate InvokeMethodDelegate = InvokeMethod;

        public static void InitializeAssembly(IntPtr classHolderPtr, string assemblyPath)
        {
            Assembly assembly = Assembly.LoadFrom(assemblyPath);

            if (assembly == null)
            {
                throw new Exception("Failed to load assembly: " + assemblyPath);
            }

            for (int i = 0; i < assembly.GetReferencedAssemblies().Length; i++)
            {
                Logger.Log(LogType.Info, "Found assembly: {0}", assembly.GetReferencedAssemblies()[i].Name);
            }

            AssemblyName hyperionCoreDependency = Array.Find(assembly.GetReferencedAssemblies(), (assemblyName) => assemblyName.Name == "HyperionCore");

            if (hyperionCoreDependency == null)
            {
                throw new Exception("Failed to find HyperionCore dependency");
            }

            string versionString = hyperionCoreDependency.Version.ToString();

            var versionParts = versionString.Split('.');

            uint majorVersion = versionParts.Length > 0 ? uint.Parse(versionParts[0]) : 0;
            uint minorVersion = versionParts.Length > 1 ? uint.Parse(versionParts[1]) : 0;
            uint patchVersion = versionParts.Length > 2 ? uint.Parse(versionParts[2]) : 0;

            uint assemblyEngineVersion = (majorVersion << 16) | (minorVersion << 8) | patchVersion;

            // Verify the engine version (major, minor)
            if (!NativeInterop_VerifyEngineVersion(assemblyEngineVersion, true, true, false))
            {
                throw new Exception("Assembly version does not match engine version");
            }

            Type[] types = assembly.GetTypes();

            foreach (Type type in types)
            {
                if (type.IsClass)
                {
                    InitManagedClass(classHolderPtr, type);
                }
            }

            NativeInterop_SetInvokeMethodFunction(classHolderPtr, Marshal.GetFunctionPointerForDelegate<InvokeMethodDelegate>(InvokeMethod));
        }

        private static ManagedClass InitManagedClass(IntPtr classHolderPtr, Type type)
        {
            ManagedClass managedClass = ManagedClass_Create(classHolderPtr, type.GetHashCode(), type.Name);

            MethodInfo[] methodInfos = type.GetMethods(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Static | BindingFlags.FlattenHierarchy);

            foreach (MethodInfo methodInfo in methodInfos)
            {
                // Skip constructors
                if (methodInfo.IsConstructor)
                {
                    continue;
                }

                // Add the objects being pointed to to the delegate cache so they don't get GC'd
                Guid guid = Guid.NewGuid();
                ManagedMethod managedMethod = managedClass.AddMethod(methodInfo.Name, guid);

                ManagedMethodCache.Instance.AddMethod(guid, methodInfo);
            }

            // Add new object, free object delegates
            managedClass.NewObjectFunction = new NewObjectDelegate(() =>
            {
                // Allocate the object
                object obj = RuntimeHelpers.GetUninitializedObject(type);

                // Call the constructor
                ConstructorInfo constructorInfo = type.GetConstructor(BindingFlags.Public | BindingFlags.Instance, null, Type.EmptyTypes, null);

                if (constructorInfo == null)
                {
                    throw new Exception("Failed to find empty constructor for type: " + type.Name);
                }

                constructorInfo.Invoke(obj, null);

                return ManagedObjectCache.Instance.AddObject(obj);
            });

            managedClass.FreeObjectFunction = new FreeObjectDelegate(FreeObject);

            return managedClass;
        }

        private static void HandleParameters(IntPtr paramsPtr, MethodInfo methodInfo, out object?[] parameters)
        {
            int numParams = methodInfo.GetParameters().Length;

            if (numParams == 0 || paramsPtr == IntPtr.Zero)
            {
                parameters = Array.Empty<object>();

                return;
            }

            parameters = new object[numParams];

            int paramsOffset = 0;

            for (int i = 0; i < numParams; i++)
            {
                Type paramType = methodInfo.GetParameters()[i].ParameterType;

                // params is stored as void**
                IntPtr paramAddress = Marshal.ReadIntPtr(paramsPtr, paramsOffset);
                paramsOffset += IntPtr.Size;

                // Helper for strings
                if (paramType == typeof(string))
                {
                    parameters[i] = Marshal.PtrToStringAnsi(Marshal.ReadIntPtr(paramAddress));

                    continue;
                }

                parameters[i] = Marshal.PtrToStructure(paramAddress, paramType);
            }
        }

        public static unsafe void InvokeMethod(Guid managedMethodGuid, Guid thisObjectGuid, IntPtr paramsPtr, IntPtr outPtr)
        {
            MethodInfo methodInfo = ManagedMethodCache.Instance.GetMethod(managedMethodGuid);

            Type returnType = methodInfo.ReturnType;
            Type thisType = methodInfo.DeclaringType;

            object[]? parameters = null;
            HandleParameters(paramsPtr, methodInfo, out parameters);

            object? thisObject = null;

            if (!methodInfo.IsStatic)
            {
                StoredManagedObject? storedObject = ManagedObjectCache.Instance.GetObject(thisObjectGuid);

                if (storedObject == null)
                {
                    throw new Exception("Failed to get target from GUID: " + thisObjectGuid);
                }

                thisObject = storedObject.obj;
            }

            object returnValue = methodInfo.Invoke(thisObject, parameters);

            if (returnType == typeof(void))
            {
                // No need to fill out the outPtr
                return;
            }

            if (returnType == typeof(string))
            {
                throw new NotImplementedException("String return type not implemented");
            }

            // write the return value to the outPtr
            // there MUST be enough space allocated at the outPtr,
            // or else this will cause memory corruption
            if (returnType.IsValueType)
            {
                Marshal.StructureToPtr(returnValue, outPtr, false);

                return;
            }

            if (returnType == typeof(IntPtr))
            {
                Marshal.WriteIntPtr(outPtr, (IntPtr)returnValue);

                return;
            }

            throw new NotImplementedException("Return type not implemented");
        }

        public static void FreeObject(ManagedObject obj)
        {
            ManagedObjectCache.Instance.RemoveObject(obj.guid);
        }

        [DllImport("libhyperion", EntryPoint = "ManagedClass_Create")]
        private static extern ManagedClass ManagedClass_Create(IntPtr classHolderPtr, int typeHash, string typeName);

        [DllImport("libhyperion", EntryPoint = "NativeInterop_VerifyEngineVersion")]
        private static extern bool NativeInterop_VerifyEngineVersion(uint assemblyEngineVersion, bool major, bool minor, bool patch);

        [DllImport("libhyperion", EntryPoint = "NativeInterop_Initialize")]
        private static extern void NativeInterop_Initialize(IntPtr invokeMethodPtr);

        [DllImport("libhyperion", EntryPoint = "NativeInterop_SetInvokeMethodFunction")]
        private static extern void NativeInterop_SetInvokeMethodFunction(IntPtr classHolderPtr, IntPtr invokeMethodPtr);
    }
}
