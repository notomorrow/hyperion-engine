using System;
using System.Collections.Generic;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public static class DeclaredObjectClass
    {
        public static Type? FindMemberType(Class ownerClass, Name memberName, bool element = false)
        {
            Type? ownerType = Class.TryGetDynamicType(ownerClass);

            if (ownerType == null)
            {
                return null;
            }

            string name = memberName.ToString();

            for (Type? type = ownerType; type != null; type = type.BaseType)
            {
                HypTypeEntry? entry = HypTypeRegistry.Get(type);

                if (entry == null)
                {
                    continue;
                }

                foreach (HypPropertyThunk thunk in entry.Properties)
                {
                    if (thunk.Name == name && thunk.MemberType != null)
                    {
                        return element ? GetElementType(thunk.MemberType) : thunk.MemberType;
                    }
                }
            }

            return null;
        }

        public static Class? Resolve(Class? typeInfoClass, Type? memberType)
        {
            if (!typeInfoClass.HasValue || memberType == null)
            {
                return typeInfoClass;
            }

            Class? declared = Class.TryGetClass(memberType);

            if (declared.HasValue && (declared.Value == typeInfoClass.Value || declared.Value.IsSubclassOf(typeInfoClass.Value)))
            {
                return declared;
            }

            return typeInfoClass;
        }

        public static object? CreateManagedInstance(Type? memberType, string className)
        {
            if (memberType == null)
            {
                return null;
            }

            Type? type = null;

            if (memberType.Name == className)
            {
                type = memberType;
            }
            else
            {
                foreach (Type candidate in memberType.Assembly.GetTypes())
                {
                    if (candidate.Name == className && memberType.IsAssignableFrom(candidate))
                    {
                        type = candidate;
                        break;
                    }
                }
            }

            if (type == null || type.IsAbstract || !Class.TryGetDynamicClass(type, out _) || type.GetConstructor(Type.EmptyTypes) == null)
            {
                return null;
            }

            return Activator.CreateInstance(type);
        }

        private static Type? GetElementType(Type type)
        {
            if (type.IsArray)
            {
                return type.GetElementType();
            }

            foreach (Type iface in type.GetInterfaces())
            {
                if (iface.IsGenericType && iface.GetGenericTypeDefinition() == typeof(IEnumerable<>))
                {
                    return iface.GetGenericArguments()[0];
                }
            }

            return null;
        }
    }
}
