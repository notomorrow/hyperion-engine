using System;
using System.Reflection;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [AttributeUsage(AttributeTargets.Method, Inherited = true)]
    public class ScriptMethodStub : Attribute
    {
    }
}
