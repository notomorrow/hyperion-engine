using System;
using System.Reflection;

namespace Hyperion
{
    [AttributeUsage(AttributeTargets.Method, Inherited = true)]
    public class ScriptMethodStub : Attribute
    {
    }

    [AttributeUsage(AttributeTargets.Class | AttributeTargets.Struct, Inherited = false)]
    public class HypClassBinding : Attribute
    {
        public string Name { get; set; }

        public HypClass HypClass
        {
            get
            {
                HypClass? hypClass = HypClass.GetClass(Name);

                if (hypClass == null || !((HypClass)hypClass).IsValid)
                {
                    throw new Exception("Failed to load HypClass: " + Name);
                }

                return (HypClass)hypClass;
            }
        }

        public static HypClassBinding? ForType(Type type, bool inheritance = false) 
        {
            Type? currentType = type;

            do
            {
                Attribute? attribute = Attribute.GetCustomAttribute((Type)currentType, typeof(HypClassBinding));

                if (attribute != null)
                {
                    return (HypClassBinding)attribute;
                }

                currentType = ((Type)currentType).BaseType;
            } while (currentType != null && inheritance);

            return null;
        }
    }
}
