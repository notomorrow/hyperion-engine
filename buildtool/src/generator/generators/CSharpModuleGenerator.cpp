/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <generator/generators/CSharpModuleGenerator.hpp>

#include <analyzer/Analyzer.hpp>
#include <analyzer/Module.hpp>

#include <parser/Parser.hpp>

#include <core/Name.hpp>

#include <core/io/ByteWriter.hpp>

namespace hyperion {
namespace buildtool {

FilePath CSharpModuleGenerator::GetOutputFilePath(const Analyzer& analyzer, const Module& mod) const
{
    FilePath relative_path = FilePath(FileSystem::RelativePath(mod.GetPath().Data(), analyzer.GetSourceDirectory().Data()).c_str());

    return analyzer.GetCSharpOutputDirectory() / relative_path.BasePath() / StringUtil::StripExtension(relative_path.Basename()) + ".cs";
}

Result CSharpModuleGenerator::Generate_Internal(const Analyzer& analyzer, const Module& mod, ByteWriter& writer) const
{
    writer.WriteString("using System;\n");
    writer.WriteString("using System.Runtime.InteropServices;\n");
    writer.WriteString("\n");
    writer.WriteString("namespace Hyperion\n");
    writer.WriteString("{\n");

    for (const Pair<String, HypClassDefinition>& pair : mod.GetHypClasses())
    {
        const HypClassDefinition& hyp_class = pair.second;

        if (const HypClassAttributeValue& attr = hyp_class.GetAttribute("NoScriptBindings"); attr.GetBool())
        {
            continue;
        }

        writer.WriteString(HYP_FORMAT("    public static class {}Extensions\n", hyp_class.name));
        writer.WriteString("    {\n");

        for (SizeType i = 0; i < hyp_class.members.Size(); ++i)
        {
            const HypMemberDefinition& member = hyp_class.members[i];

            if (const HypClassAttributeValue& attr = member.GetAttribute("NoScriptBindings"); attr.GetBool())
            {
                // skip generating script bindings for this
                continue;
            }

            String managed_name = member.name;

            if (const HypClassAttributeValue& attr = member.GetAttribute("ManagedName"); attr.IsValid() && attr.IsString())
            {
                managed_name = attr.GetString();
            }

            if (member.type == HypMemberType::TYPE_METHOD)
            {
                if (!member.cxx_type->is_function)
                {
                    return HYP_MAKE_ERROR(Error, "Cannot generate script bindings for non-function type");
                }

                if (member.cxx_type->is_static)
                {
                    continue;
                }

                const ASTFunctionType* function_type = dynamic_cast<const ASTFunctionType*>(member.cxx_type.Get());

                if (!function_type)
                {
                    return HYP_MAKE_ERROR(Error, "Internal error: failed cast to ASTFunctionType");
                }

                String return_type_name;

                if (TResult<String> res = MapToCSharpType(function_type->return_type); res.HasError())
                {
                    return res.GetError();
                }
                else
                {
                    return_type_name = res.GetValue();
                }

                Array<String> method_arg_decls;
                Array<String> method_arg_names;

                for (SizeType i = 0; i < function_type->parameters.Size(); ++i)
                {
                    const ASTMemberDecl* parameter = function_type->parameters[i];

                    String parameter_type_name;

                    if (TResult<String> res = MapToCSharpType(parameter->type); res.HasError())
                    {
                        return res.GetError();
                    }
                    else
                    {
                        parameter_type_name = res.GetValue();
                    }

                    method_arg_decls.PushBack(HYP_FORMAT("{} {}", parameter_type_name, parameter->name));
                    method_arg_names.PushBack(parameter->name);
                }

                writer.WriteString(HYP_FORMAT("        public static {} {}(this {} obj{})\n", return_type_name, managed_name, hyp_class.name, method_arg_decls.Any() ? ", " + String::Join(method_arg_decls, ", ") : ""));
                writer.WriteString("        {\n");

                if (function_type->return_type->IsVoid())
                {
                    if (hyp_class.type == HypClassDefinitionType::STRUCT)
                    {
                        writer.WriteString(HYP_FORMAT("            HypObject.GetMethod(HypClass.GetClass<{}>(), new Name({})).InvokeNative(obj{});\n",
                            hyp_class.name, uint64(CreateWeakNameFromDynamicString(member.name.Data())), method_arg_names.Any() ? ", " + String::Join(method_arg_names, ", ") : ""));
                    }
                    else if (hyp_class.type == HypClassDefinitionType::CLASS)
                    {
                        writer.WriteString(HYP_FORMAT("            obj.GetMethod(new Name({})).InvokeNative(obj{});\n",
                            uint64(CreateWeakNameFromDynamicString(member.name.Data())), method_arg_names.Any() ? ", " + String::Join(method_arg_names, ", ") : ""));
                    }
                    else
                    {
                        return HYP_MAKE_ERROR(Error, "Unsupported HypClass type");
                    }
                }
                else
                {
                    if (hyp_class.type == HypClassDefinitionType::STRUCT)
                    {
                        writer.WriteString(HYP_FORMAT("            using (HypDataBuffer resultData = HypObject.GetMethod(HypClass.GetClass<{}>(), new Name({})).InvokeNative(obj{}))\n",
                            hyp_class.name, uint64(CreateWeakNameFromDynamicString(member.name.Data())), method_arg_names.Any() ? ", " + String::Join(method_arg_names, ", ") : ""));
                        writer.WriteString("            {\n");
                        writer.WriteString(HYP_FORMAT("                return ({})resultData.GetValue();\n", return_type_name));
                        writer.WriteString("            }\n");
                    }
                    else if (hyp_class.type == HypClassDefinitionType::CLASS)
                    {
                        writer.WriteString(HYP_FORMAT("            using (HypDataBuffer resultData = obj.GetMethod(new Name({})).InvokeNative(obj{}))\n",
                            uint64(CreateWeakNameFromDynamicString(member.name.Data())), method_arg_names.Any() ? ", " + String::Join(method_arg_names, ", ") : ""));
                        writer.WriteString("            {\n");
                        writer.WriteString(HYP_FORMAT("                return ({})resultData.GetValue();\n", return_type_name));
                        writer.WriteString("            }\n");
                    }
                    else
                    {
                        return HYP_MAKE_ERROR(Error, "Unsupported HypClass type");
                    }
                }

                writer.WriteString("        }\n");

                continue;
            }

            // Generate code to get a ScriptableDelegate C# object corresponding to the C++ object
            if (member.type == HypMemberType::TYPE_FIELD && member.cxx_type->IsScriptableDelegate())
            {
                writer.WriteString(HYP_FORMAT("        public static ScriptableDelegate Get{}Delegate(this {} obj)\n", managed_name, hyp_class.name));
                writer.WriteString("        {\n");

                writer.WriteString(HYP_FORMAT("            HypField field = (HypField)obj.HypClass.GetField(new Name({}));\n", uint64(CreateWeakNameFromDynamicString(member.name.Data()))));
                writer.WriteString("            IntPtr fieldAddress = obj.NativeAddress + ((IntPtr)((HypField)field).Offset);\n\n");
                writer.WriteString("            return new ScriptableDelegate(obj, fieldAddress);\n");

                writer.WriteString("        }\n");

                continue;
            }
        }

        writer.WriteString("    }\n");
    }

    writer.WriteString("}\n");

    return {};
}

} // namespace buildtool
} // namespace hyperion