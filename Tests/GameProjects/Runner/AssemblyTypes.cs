using System.Reflection.Metadata;
using System.Reflection.PortableExecutable;

namespace Hyperion.GameProjectTests;

public sealed record AssemblyType(
    string FullName,
    string? BaseTypeName,
    string? ClassBindingName,
    IReadOnlyList<string> PropertyNames,
    IReadOnlyList<string> MethodNames);

/// The types an assembly declares, read from its metadata rather than loaded, since a bindings assembly's references
/// (Hyperion.NET.*) only load inside the engine
public static class AssemblyTypes
{
    public static IReadOnlyDictionary<string, AssemblyType> Read(string assemblyPath)
    {
        using FileStream stream = File.OpenRead(assemblyPath);
        using PEReader peReader = new PEReader(stream);

        MetadataReader reader = peReader.GetMetadataReader();
        Dictionary<string, AssemblyType> types = new Dictionary<string, AssemblyType>();

        foreach (TypeDefinitionHandle handle in reader.TypeDefinitions)
        {
            TypeDefinition type = reader.GetTypeDefinition(handle);
            string fullName = QualifiedName(reader.GetString(type.Namespace), reader.GetString(type.Name));

            types[fullName] = new AssemblyType(
                fullName,
                GetTypeName(reader, type.BaseType),
                FindClassBindingName(reader, type),
                type.GetProperties().Select(property => reader.GetString(reader.GetPropertyDefinition(property).Name)).ToList(),
                type.GetMethods().Select(method => reader.GetString(reader.GetMethodDefinition(method).Name)).ToList());
        }

        return types;
    }

    private static string QualifiedName(string ns, string name)
    {
        return ns.Length == 0 ? name : ns + "." + name;
    }

    private static string? GetTypeName(MetadataReader reader, EntityHandle handle)
    {
        if (handle.IsNil)
        {
            return null;
        }

        switch (handle.Kind)
        {
        case HandleKind.TypeReference:
            TypeReference reference = reader.GetTypeReference((TypeReferenceHandle)handle);
            return QualifiedName(reader.GetString(reference.Namespace), reader.GetString(reference.Name));
        case HandleKind.TypeDefinition:
            TypeDefinition definition = reader.GetTypeDefinition((TypeDefinitionHandle)handle);
            return QualifiedName(reader.GetString(definition.Namespace), reader.GetString(definition.Name));
        default:
            return null;
        }
    }

    // [ClassBinding(Name = "...")]: the native class a managed type wraps
    private static string? FindClassBindingName(MetadataReader reader, TypeDefinition type)
    {
        foreach (CustomAttributeHandle attributeHandle in type.GetCustomAttributes())
        {
            CustomAttribute attribute = reader.GetCustomAttribute(attributeHandle);

            EntityHandle attributeType = attribute.Constructor.Kind switch
            {
                HandleKind.MemberReference => reader.GetMemberReference((MemberReferenceHandle)attribute.Constructor).Parent,
                HandleKind.MethodDefinition => reader.GetMethodDefinition((MethodDefinitionHandle)attribute.Constructor).GetDeclaringType(),
                _ => default
            };

            if (GetTypeName(reader, attributeType) != "Hyperion.ClassBinding")
            {
                continue;
            }

            CustomAttributeValue<string> value = attribute.DecodeValue(new StringTypeProvider());

            foreach (CustomAttributeNamedArgument<string> argument in value.NamedArguments)
            {
                if (argument.Name == "Name")
                {
                    return argument.Value as string;
                }
            }

            return string.Empty;
        }

        return null;
    }

    // only names types, which is all decoding a [ClassBinding]'s string arguments needs
    private sealed class StringTypeProvider : ICustomAttributeTypeProvider<string>
    {
        public string GetPrimitiveType(PrimitiveTypeCode typeCode) => typeCode.ToString();
        public string GetSystemType() => "System.Type";
        public string GetSZArrayType(string elementType) => elementType + "[]";
        public string GetTypeFromDefinition(MetadataReader reader, TypeDefinitionHandle handle, byte rawTypeKind) => GetTypeName(reader, handle) ?? "";
        public string GetTypeFromReference(MetadataReader reader, TypeReferenceHandle handle, byte rawTypeKind) => GetTypeName(reader, handle) ?? "";
        public string GetTypeFromSerializedName(string name) => name;
        public PrimitiveTypeCode GetUnderlyingEnumType(string type) => PrimitiveTypeCode.Int32;
        public bool IsSystemType(string type) => type == "System.Type";
    }
}
