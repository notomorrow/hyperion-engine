/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <analyzer/EngineTypes.hpp>
#include <analyzer/Analyzer.hpp>
#include <analyzer/Module.hpp>

#include <Core/DataProcessing/JSON/JSON.hpp>

#include <Core/IO/ByteReader.hpp>
#include <Core/IO/ByteWriter.hpp>

#include <util/Util.hpp>

#include <algorithm>

namespace Hyperion {
namespace CodeGen {

static const ClassDefinitionType s_referenceTypes[] = { ClassDefinitionType::Class, ClassDefinitionType::Struct, ClassDefinitionType::Enum };

static JSON::Value AttributeValueToJSON(const ClassAttributeValue& value)
{
    if (value.IsBool())
    {
        return JSON::Value(value.GetBool());
    }

    if (value.IsInt())
    {
        return JSON::Value(int32(value.GetInt()));
    }

    return JSON::Value(JSON::JString(value.GetString()));
}

static ClassAttributeValue AttributeValueFromJSON(const JSON::Value& value)
{
    if (value.IsBool())
    {
        return ClassAttributeValue(value.AsBool());
    }

    if (value.IsNumber())
    {
        return ClassAttributeValue(int(value.ToInt32()));
    }

    return ClassAttributeValue(value.ToString());
}

Result WriteEngineTypes(const Analyzer& analyzer, const Set<String>& csharpClassNames, const FilePath& filePath)
{
    Array<const ClassDefinition*> definitions;

    for (const UniquePtr<Module>& mod : analyzer.GetModules())
    {
        for (const Pair<String, ClassDefinition>& pair : mod->GetClasses())
        {
            definitions.PushBack(&pair.second);
        }
    }

    std::sort(definitions.Begin(), definitions.End(), [](const ClassDefinition* a, const ClassDefinition* b)
        {
            return a->name < b->name;
        });

    JSON::JArray types;

    for (const ClassDefinition* definition : definitions)
    {
        JSON::JArray namespaceParts;

        for (const String& part : definition->namespaceParts)
        {
            namespaceParts.PushBack(JSON::JString(part));
        }

        JSON::JArray bases;

        for (const String& baseClassName : definition->baseClassNames)
        {
            bases.PushBack(JSON::JString(baseClassName));
        }

        JSON::Object attributes;

        for (const Pair<String, ClassAttributeValue>& attribute : definition->attributes)
        {
            attributes.Set(attribute.first, AttributeValueToJSON(attribute.second));
        }

        JSON::Object entry;
        entry.Set("name", JSON::JString(definition->name));
        entry.Set("kind", JSON::JString(ClassDefinitionTypeToString(definition->type)));
        entry.Set("namespace", JSON::Value(std::move(namespaceParts)));
        entry.Set("bases", JSON::Value(std::move(bases)));
        entry.Set("attributes", JSON::Value(std::move(attributes)));
        entry.Set("csharp", JSON::Value(csharpClassNames.Contains(definition->name)));

        types.PushBack(JSON::Value(std::move(entry)));
    }

    JSON::Object root;
    root.Set("version", JSON::JString(HYP_FORMAT("{}.{}.{}", HYP_CODEGEN_VERSION_MAJOR, HYP_CODEGEN_VERSION_MINOR, HYP_CODEGEN_VERSION_PATCH)));
    root.Set("types", JSON::Value(std::move(types)));

    FilePath tmpFilePath = filePath + ".tmp";

    {
        FileByteWriter writer { tmpFilePath };

        if (!writer.IsOpen())
        {
            return HYP_MAKE_ERROR(Error, "Failed to open engine types file {} for writing", tmpFilePath);
        }

        writer.WriteString(JSON::Value(std::move(root)).ToString(true).ToUtf8());
        writer.Close();
    }

    return ReplaceFileIfDifferent(tmpFilePath, filePath);
}

Result LoadEngineTypes(Analyzer& analyzer, const FilePath& filePath)
{
    FileByteReader reader { filePath };

    if (reader.Eof())
    {
        return HYP_MAKE_ERROR(Error, "Engine types file {} is missing or empty", filePath);
    }

    const JSON::ParseResult parseResult = JSON::Parse(reader);

    if (!parseResult.ok)
    {
        return HYP_MAKE_ERROR(Error, "Failed to parse engine types file {}: {}", filePath, parseResult.message);
    }

    const String expectedVersion = HYP_FORMAT("{}.{}.{}", HYP_CODEGEN_VERSION_MAJOR, HYP_CODEGEN_VERSION_MINOR, HYP_CODEGEN_VERSION_PATCH);

    if (parseResult.value["version"].ToString() != expectedVersion)
    {
        return HYP_MAKE_ERROR(Error, "Engine types file {} was written by CodeGen {}, this is {}", filePath, parseResult.value["version"].ToString(), expectedVersion);
    }

    if (!parseResult.value["types"].IsArray())
    {
        return HYP_MAKE_ERROR(Error, "Engine types file {} has no types", filePath);
    }

    for (const JSON::Value& entry : parseResult.value["types"].ToArray())
    {
        ClassDefinition definition;
        definition.name = entry["name"].ToString();

        const String kind = entry["kind"].ToString();

        for (ClassDefinitionType type : s_referenceTypes)
        {
            if (ClassDefinitionTypeToString(type) == kind)
            {
                definition.type = type;
            }
        }

        if (definition.name.Empty() || definition.type == ClassDefinitionType::None)
        {
            return HYP_MAKE_ERROR(Error, "Engine types file {} has an invalid entry '{}' ({})", filePath, definition.name, kind);
        }

        if (entry["namespace"].IsArray())
        {
            for (const JSON::Value& part : entry["namespace"].ToArray())
            {
                definition.namespaceParts.PushBack(part.ToString());
            }
        }

        if (entry["bases"].IsArray())
        {
            for (const JSON::Value& baseClassName : entry["bases"].ToArray())
            {
                definition.baseClassNames.PushBack(baseClassName.ToString());
            }
        }

        if (entry["attributes"].IsObject())
        {
            for (const auto& attribute : entry["attributes"].AsObject())
            {
                definition.attributes.PushBack({ attribute.first, AttributeValueFromJSON(attribute.second) });
            }
        }

        definition.isCXXClass = definition.type == ClassDefinitionType::Class;
        definition.isCXXStruct = definition.type == ClassDefinitionType::Struct;
        definition.isCXXEnumClass = definition.type == ClassDefinitionType::Enum;
        definition.hasCSharpClass = entry["csharp"].ToBool();

        const String name = definition.name;

        analyzer.GetEngineTypes().Set(name, std::move(definition));
    }

    return {};
}

} // namespace CodeGen
} // namespace Hyperion
