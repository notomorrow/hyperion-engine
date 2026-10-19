/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <generator/generators/CBindingGenerator.hpp>
#include <generator/generators/BindingsShared.hpp>

#include <analyzer/Analyzer.hpp>
#include <analyzer/Module.hpp>

#include <parser/Parser.hpp>

#include <util/Util.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Reflection/Class.hpp>

#include <Core/Utilities/StringUtil.hpp>

#include <Core/IO/ByteWriter.hpp>

#include <utility>

namespace Hyperion {
namespace CodeGen {

using namespace BindingsShared;

namespace {

String UnwrappedTypeName(const ASTType* type)
{
    const ASTType* unwrapped = (type->isLvalueReference || type->isRvalueReference) ? type->refTo.Get() : type;

    return unwrapped->typeName->ToString(/* includeNamespace */ false);
}

TResult<StrataTypeMapping> BuildThunkInputParam(const Analyzer& analyzer, const Set<String>& allHandleNames,
    const ASTType* paramType, const String& paramName, Array<String>& sigParams, Array<String>& callArgs)
{
    TResult<StrataTypeMapping> paramRes = MapToStrataType(analyzer, paramType);

    if (paramRes.HasError())
    {
        return paramRes;
    }

    const StrataTypeMapping paramTypeMapping = paramRes.GetValue();

    if ((paramTypeMapping.isHandle || paramTypeMapping.isEnum)
        && !allHandleNames.Contains(paramTypeMapping.typeName))
    {
        return HYP_MAKE_ERROR(Error, "Type '{}' is not a declared Strata {}", paramTypeMapping.typeName,
            paramTypeMapping.isEnum ? "enum" : "handle");
    }

    const ASTType* unwrappedParamType = (paramType->isLvalueReference || paramType->isRvalueReference)
        ? paramType->refTo.Get()
        : paramType;

    if (paramTypeMapping.isString)
    {
        // Strata strings have no length; pass the raw char*.
        const String stringCxxTypeName = unwrappedParamType->typeName->ToString(/* includeNamespace */ false);

        sigParams.PushBack(HYP_FORMAT("const char* {}", paramName));
        callArgs.PushBack(HYP_FORMAT("{}({})", stringCxxTypeName, paramName));
    }
    else if (paramTypeMapping.isArray)
    {
        // Copy the {ptr, u64} fat into an engine array.
        const String elementCxxType = unwrappedParamType->templateArguments[0]->type->Format();
        const String arrayCxxTypeName = unwrappedParamType->typeName->ToString(/* includeNamespace */ false);

        sigParams.PushBack(HYP_FORMAT("::Hyperion::Strata::SArray<{}>* {}", elementCxxType, paramName));
        callArgs.PushBack(HYP_FORMAT("{}<{}>({}->data, size_t({}->length))", arrayCxxTypeName, elementCxxType, paramName, paramName));
    }
    else if (paramTypeMapping.isVector)
    {
        const String vectorCxxTypeName = unwrappedParamType->typeName->ToString(/* includeNamespace */ false);

        sigParams.PushBack(HYP_FORMAT("const {}* {}", vectorCxxTypeName, paramName));
        callArgs.PushBack(HYP_FORMAT("::Hyperion::Strata::LoadUnaligned({})", paramName));
    }
    else if (paramTypeMapping.isStructValue)
    {
        sigParams.PushBack(HYP_FORMAT("{}* {}", paramTypeMapping.CxxTypeName(), paramName));

        if (paramType->isPointer)
        {
            callArgs.PushBack(paramName);
        }
        else if (paramType->isLvalueReference && !(paramType->refTo && paramType->refTo->isConst))
        {
            callArgs.PushBack(HYP_FORMAT("*{}", paramName));
        }
        else
        {
            callArgs.PushBack(HYP_FORMAT("::Hyperion::Strata::LoadUnaligned({})", paramName));
        }
    }
    else
    {
        sigParams.PushBack(paramType->FormatDecl(paramName));
        callArgs.PushBack(paramName);
    }

    return paramRes;
}

// Formats the `extern "C"` thunk forwarding callExpr to the host boundary.
String FormatThunkDefinition(const String& externName, const StrataTypeMapping& returnTypeMapping,
    const ASTType* cxxReturnType, const String& sigParamsString, const String& callExpr)
{
    String methodOutput;

    if (returnTypeMapping.isArray)
    {
        methodOutput += HYP_FORMAT("extern \"C\" HYP_EXPORT void {}({})", externName, sigParamsString);
        methodOutput += " { const auto& hypReturnValue = ";
        methodOutput += callExpr;
        methodOutput += "; ::Hyperion::Strata::SetReturnArray(outReturn, hypReturnValue.Data(), hypReturnValue.Size()); }\n";
    }
    else if (returnTypeMapping.isStructValue || returnTypeMapping.isVector)
    {
        methodOutput += HYP_FORMAT("extern \"C\" HYP_EXPORT void {}({})", externName, sigParamsString);
        methodOutput += " { ::Hyperion::Strata::StoreUnaligned(outReturn, ";
        methodOutput += callExpr;
        methodOutput += "); }\n";
    }
    else if (returnTypeMapping.isString)
    {
        // `string` returns come back through the out-param ({ptr, len} fat).
        methodOutput += HYP_FORMAT("extern \"C\" HYP_EXPORT void {}({})", externName, sigParamsString);
        methodOutput += " { ::Hyperion::Strata::SetReturnString(outReturn, ";
        methodOutput += callExpr;
        methodOutput += "); }\n";
    }
    else if (cxxReturnType == nullptr || cxxReturnType->IsVoid())
    {
        methodOutput += HYP_FORMAT("extern \"C\" HYP_EXPORT void {}({})", externName, sigParamsString);
        methodOutput += " { ";
        methodOutput += callExpr;
        methodOutput += "; }\n";
    }
    else
    {
        const String returnTypeString = cxxReturnType->Format();

        methodOutput += HYP_FORMAT("extern \"C\" HYP_EXPORT {} {}({})", returnTypeString, externName, sigParamsString);
        methodOutput += " { return ";
        methodOutput += callExpr;
        methodOutput += "; }\n";
    }

    return methodOutput;
}

} // anonymous namespace

Result CBindingGenerator::EmitThunks(const Analyzer& analyzer, const Module& mod, const Set<String>& allHandleNames, ByteWriter& writer) const
{
    bool wroteIncludes = false;

    auto ensureIncludes = [&]()
    {
        if (!wroteIncludes)
        {
            writer.WriteString("\n#ifdef HYP_STRATA\n#include <Core/Scripting/Strata/ThunkDrawer.hpp>\n#endif\n");
            writer.WriteString("#include <Core/Scripting/Strata/StrataMarshal.hpp>\n\n");

            wroteIncludes = true;
        }
    };

    for (const Pair<String, ClassDefinition>& pair : mod.GetClasses())
    {
        const ClassDefinition& cls = pair.second;

        if ((cls.type != ClassDefinitionType::Class && cls.type != ClassDefinitionType::Struct
                && cls.type != ClassDefinitionType::Enum)
            || !IsStrataScriptable(analyzer, cls))
        {
            continue;
        }

        Set<String> emittedExternNames;
        String classThunks;

        for (const MethodThunk& thunk : CollectMethodThunks(analyzer, cls, allHandleNames))
        {
            const MemberDef& member = *thunk.member;
            const ASTFunctionType* functionType = thunk.functionType;
            const bool isStatic = thunk.isStatic;
            const StrataTypeMapping& returnTypeMapping = thunk.returnMapping;

            Array<String> sigParams; // "Type name" for the thunk signature
            Array<String> callArgs;  // "name" for the forwarded call

            for (size_t j = 0; j < functionType->parameters.Size(); ++j)
            {
                const ASTMemberDecl* parameter = functionType->parameters[j];

                const String paramName = parameter->name.Any() ? parameter->name : HYP_FORMAT("arg{}", j);

                (void)BuildThunkInputParam(analyzer, allHandleNames, parameter->type.Get(), paramName, sigParams, callArgs);
            }

            const String managedName = ResolveManagedName(member);
            const String& externName = thunk.symbol;

            emittedExternNames.Insert(externName);

            // Chained setters emit void-shaped; the fluent return is discarded.
            const bool propertySetterIdiom = thunk.propertySetterIdiom;
            const bool returnsViaOutParam = thunk.returnsViaOutParam;

            // Build the signature param list: [<Class>* self, ]<params...>[, <Ret>(*) outReturn]
            Array<String> allSigParams;

            if (!isStatic)
            {
                allSigParams.PushBack(HYP_FORMAT("{}* self", cls.name));
            }

            for (const String& sigParam : sigParams)
            {
                allSigParams.PushBack(sigParam);
            }

            if (returnTypeMapping.isArray)
            {
                // Reference-stripping like params: getters return `const Array<T>&`.
                const ASTType* unwrappedReturnType = (functionType->returnType->isLvalueReference || functionType->returnType->isRvalueReference)
                    ? functionType->returnType->refTo.Get()
                    : functionType->returnType.Get();

                const String returnElementCxxType = unwrappedReturnType->templateArguments[0]->type->Format();

                allSigParams.PushBack(HYP_FORMAT("::Hyperion::Strata::SArray<{}>* outReturn", returnElementCxxType));
            }
            else if (returnTypeMapping.isString)
            {
                // `string` is a {ptr, len} fat; the host writes it via SetReturnString.
                allSigParams.PushBack("::Hyperion::Strata::SString* outReturn");
            }
            else if (returnTypeMapping.isVector && returnsViaOutParam)
            {
                allSigParams.PushBack(HYP_FORMAT("{}* outReturn", UnwrappedTypeName(functionType->returnType.Get())));
            }
            else if (returnsViaOutParam)
            {
                allSigParams.PushBack(HYP_FORMAT("{}* outReturn", returnTypeMapping.CxxTypeName()));
            }

            const String sigParamsString = allSigParams.Any() ? String::Join(allSigParams, ", ") : String("");
            const String callArgsString = callArgs.Any() ? String::Join(callArgs, ", ") : String("");

            const String callExpr = isStatic
                ? HYP_FORMAT("{}::{}({})", cls.name, member.name, callArgsString)
                : HYP_FORMAT("self->{}({})", member.name, callArgsString);

            // Wrap the thunk + registrar in the method's preprocessor condition.
            const StrataTypeMapping thunkReturnMapping = propertySetterIdiom
                ? StrataTypeMapping { "void" }
                : returnTypeMapping;

            String methodOutput = FormatThunkDefinition(externName, thunkReturnMapping,
                propertySetterIdiom ? nullptr : functionType->returnType.Get(), sigParamsString, callExpr);

            // Register for static init.
            methodOutput += HYP_FORMAT("#ifdef HYP_STRATA\nstatic const bool s_strataBinding_{}_{} = ::Hyperion::Strata::ThunkDrawer::Register(\"{}\"_sh, reinterpret_cast<void*>(&{}_{}));\n#endif\n",
                cls.name, managedName, externName, cls.name, managedName);
            if (member.condition.Any())
            {
                classThunks += HYP_FORMAT("#if {}\n", member.condition);
            }

            classThunks += methodOutput;

            if (member.condition.Any())
            {
                classThunks += HYP_FORMAT("#endif // {}\n", member.condition);
            }

            classThunks += "\n";
        }

        // Property accessor thunks; reused externs need none.
        {
            for (const ResolvedStrataProperty& prop : CollectImplProperties(analyzer, cls, allHandleNames))
            {
                PropertyAccessorBinding binding;

                if (!ResolvePropertyAccessorBinding(analyzer, cls, prop, allHandleNames, emittedExternNames, binding))
                {
                    continue;
                }

                const String selfParam = HYP_FORMAT("{}* self", cls.name);

                if (binding.getterSynthesized)
                {
                    const String getterCall = prop.getter->cxxType->isFunction
                        ? HYP_FORMAT("self->{}()", prop.getter->name)
                        : HYP_FORMAT("self->{}", prop.getter->name);

                    // String getters write the {ptr, len} fat through an out-param.
                    // -- Why? They don't have to, anymore --
                    // They don't have to per se; but because strata extern char* returns do NOT hand off ownership,
                    // we do it this way so ownership is created on the C++ side and strata takes it over.
                    Array<String> getterSigParams;
                    getterSigParams.PushBack(selfParam);

                    if (prop.mapping.isString)
                    {
                        getterSigParams.PushBack("::Hyperion::Strata::SString* outReturn");
                    }
                    else if (prop.mapping.isVector)
                    {
                        getterSigParams.PushBack(HYP_FORMAT("{}* outReturn", UnwrappedTypeName(prop.getterValueType)));
                    }

                    String getterThunk = FormatThunkDefinition(prop.getterSymbol, prop.mapping, prop.getterValueType,
                        String::Join(getterSigParams, ", "), getterCall);
                    getterThunk += HYP_FORMAT("#ifdef HYP_STRATA\nstatic const bool s_strataPropertyBinding_{}_get_{} = ::Hyperion::Strata::ThunkDrawer::Register(\"{}\"_sh, reinterpret_cast<void*>(&{}));\n#endif\n",
                        cls.name, prop.name, prop.getterSymbol, prop.getterSymbol);

                    const String condition = PropertyAccessorCondition(prop, prop.getter);

                    if (condition.Any())
                    {
                        classThunks += HYP_FORMAT("#if {}\n", condition);
                    }

                    classThunks += getterThunk;

                    if (condition.Any())
                    {
                        classThunks += HYP_FORMAT("#endif // {}\n", condition);
                    }

                    classThunks += "\n";

                    emittedExternNames.Insert(binding.getterSymbol);
                }

                if (binding.setterSynthesized)
                {
                    Array<String> sigParams;
                    Array<String> callArgs;
                    sigParams.PushBack(selfParam);

                    TResult<StrataTypeMapping> valueRes = BuildThunkInputParam(analyzer, allHandleNames, prop.setterValueType, "value", sigParams, callArgs);

                    if (!valueRes.HasError())
                    {
                        const String setterCall = prop.setter->cxxType->isFunction
                            ? HYP_FORMAT("self->{}({})", prop.setter->name, String::Join(callArgs, ", "))
                            : HYP_FORMAT("self->{} = {}", prop.setter->name, callArgs[0]);

                        String setterThunk = FormatThunkDefinition(prop.setterSymbol, StrataTypeMapping { "void" }, nullptr, String::Join(sigParams, ", "), setterCall);
                        setterThunk += HYP_FORMAT("#ifdef HYP_STRATA\nstatic const bool s_strataPropertyBinding_{}_set_{} = ::Hyperion::Strata::ThunkDrawer::Register(\"{}\"_sh, reinterpret_cast<void*>(&{}));\n#endif\n",
                            cls.name, prop.name, prop.setterSymbol, prop.setterSymbol);

                        const String condition = PropertyAccessorCondition(prop, prop.setter);

                        if (condition.Any())
                        {
                            classThunks += HYP_FORMAT("#if {}\n", condition);
                        }

                        classThunks += setterThunk;

                        if (condition.Any())
                        {
                            classThunks += HYP_FORMAT("#endif // {}\n", condition);
                        }

                        classThunks += "\n";

                        emittedExternNames.Insert(binding.setterSymbol);
                    }
                }
            }
        }

        if (!classThunks.Any())
        {
            continue;
        }

        ensureIncludes();

        if (cls.condition.Any())
        {
            writer.WriteString(HYP_FORMAT("#if {}\n\n", cls.condition));
        }

        writer.WriteString(classThunks);

        if (cls.condition.Any())
        {
            writer.WriteString(HYP_FORMAT("#endif // {}\n\n", cls.condition));
        }
    }

    return {};
}

} // namespace CodeGen
} // namespace Hyperion
