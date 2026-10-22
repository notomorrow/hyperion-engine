using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [AttributeUsage(AttributeTargets.Method, Inherited = true)]
    public class ScriptMethodStub : Attribute
    {
    }

    /// <summary>
    /// Indicates that a struct does not have a native class representation. Used by the NativeInterop system to avoid trying to hook up a native class for this type.
    /// </summary>
    [AttributeUsage(AttributeTargets.Struct, Inherited = false)]
    public class NoNativeClass : Attribute
    {
    }

    /// <summary>
    /// Indicates that a method or property should only be called from the main thread.
    /// </summary>
    [AttributeUsage(AttributeTargets.Method | AttributeTargets.Property, Inherited = true)]
    public class MainThreadOnly : Attribute
    {
    }

    /// <summary>
    /// Indicates that a method or property should only be called from the simulation thread.
    /// </summary>
    [AttributeUsage(AttributeTargets.Method | AttributeTargets.Property, Inherited = true)]
    public class SimThreadOnly : Attribute
    {
    }

    /// <summary>
    /// Base for the C# counterparts of HYP_FIELD / HYP_PROPERTY. Named arguments use the same names as the C++ macros,
    /// and become native class attributes by lowercasing them (as CodeGen does), so only arguments that were set are emitted.
    /// </summary>
    public abstract class HypMemberAttribute : Attribute
    {
        private readonly Dictionary<string, object> values = new Dictionary<string, object>();

        public bool Serialize { get => GetBool("serialize", true); set => values["serialize"] = value; }
        public bool Transient { get => GetBool("transient", false); set => values["transient"] = value; }
        public bool Editor { get => GetBool("editor", true); set => values["editor"] = value; }
        public int EditorOrder { get => values.TryGetValue("editororder", out object? v) ? (int)v : 0; set => values["editororder"] = value; }
        public bool EditEnabled { get => GetBool("editenabled", true); set => values["editenabled"] = value; }
        public string? EditCondition { get => values.TryGetValue("editcondition", out object? v) ? (string)v : null; set => SetString("editcondition", value); }
        public string? Label { get => values.TryGetValue("label", out object? v) ? (string)v : null; set => SetString("label", value); }
        public string? Description { get => values.TryGetValue("description", out object? v) ? (string)v : null; set => SetString("description", value); }
        public bool JsonIgnore { get => GetBool("jsonignore", false); set => values["jsonignore"] = value; }
        public bool SaveAsReference { get => GetBool("saveasreference", false); set => values["saveasreference"] = value; }
        public bool Replicated { get => GetBool("replicated", false); set => values["replicated"] = value; }

        public IReadOnlyDictionary<string, object> NativeAttributes => values;

        private bool GetBool(string name, bool defaultValue)
        {
            return values.TryGetValue(name, out object? v) ? (bool)v : defaultValue;
        }

        private void SetString(string name, string? value)
        {
            if (value == null)
                values.Remove(name);
            else
                values[name] = value;
        }
    }

    /// <summary>
    /// C# counterpart of HYP_FIELD(...): reflects a field of a script or ObjectBase subclass so it is saved and shown in the editor.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field, Inherited = true)]
    public sealed class HypField : HypMemberAttribute
    {
    }

    /// <summary>
    /// C# counterpart of HYP_PROPERTY(...): reflects a property of a script or ObjectBase subclass so it is saved and shown in the editor.
    /// </summary>
    [AttributeUsage(AttributeTargets.Property, Inherited = true)]
    public sealed class HypProperty : HypMemberAttribute
    {
    }
}
