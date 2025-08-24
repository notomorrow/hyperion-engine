#include <script/ScriptApi.hpp>
#include <script/ScriptApi2.hpp>

#include <script/compiler/Lexer.hpp>
#include <script/compiler/Parser.hpp>
#include <script/compiler/TokenStream.hpp>
#include <script/compiler/ast/AstAsExpression.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstTemplateExpression.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>

#include <script/vm/VM.hpp>

#include <core/memory/ByteBuffer.hpp>

namespace hyperion {
namespace scriptapi2 {

// ClassBuilder

ClassBuilder::ClassBuilder(Context* context, ClassDefinition classDefinition)
    : m_context(context),
      m_classDefinition(std::move(classDefinition))
{
    Assert(m_context != nullptr);
}

ClassBuilder& ClassBuilder::Member(String name, String typeString,
    Value value)
{
    m_classDefinition.members.PushBack(Symbol { name, typeString, value });

    return *this;
}

ClassBuilder& ClassBuilder::Method(String name, String typeString,
    NativeFunctionPtr_t fn)
{
    m_classDefinition.members.PushBack(Symbol { name, typeString, fn });

    return *this;
}

ClassBuilder& ClassBuilder::StaticMember(String name, String typeString,
    Value value)
{
    m_classDefinition.staticMembers.PushBack(Symbol { name, typeString, value });

    return *this;
}

ClassBuilder& ClassBuilder::StaticMethod(String name, String typeString,
    NativeFunctionPtr_t fn)
{
    m_classDefinition.staticMembers.PushBack(Symbol { name, typeString, fn });

    return *this;
}

void ClassBuilder::Build()
{
    Mutex::Guard guard(m_context->m_mutex);

    // Add `nativeTypeId` member to class
    m_classDefinition.staticMembers.PushBack(
        { "nativeTypeId",
            { "uint" },
            vm::Value { vm::Value::U32,
                { .u32 = m_classDefinition.nativeTypeId.Value() } } });

    m_context->m_classDefinitions.PushBack(std::move(m_classDefinition));
}

// Context

Context& Context::Global(String name, String typeString, Value value)
{
    Mutex::Guard guard(m_mutex);

    m_globals.PushBack(GlobalDefinition { Symbol { name, typeString, value } });

    return *this;
}

Context& Context::Global(String name, String genericParamsString,
    String typeString, Value value)
{
    Mutex::Guard guard(m_mutex);

    m_globals.PushBack(GlobalDefinition { Symbol { name, typeString, value },
        std::move(genericParamsString) });

    return *this;
}

Context& Context::Global(String name, String typeString,
    NativeFunctionPtr_t fn)
{
    Mutex::Guard guard(m_mutex);

    m_globals.PushBack(GlobalDefinition { Symbol { name, typeString, fn } });

    return *this;
}

Context& Context::Global(String name, String genericParamsString,
    String typeString, NativeFunctionPtr_t fn)
{
    Mutex::Guard guard(m_mutex);

    m_globals.PushBack(GlobalDefinition { Symbol { name, typeString, fn },
        std::move(genericParamsString) });

    return *this;
}

RC<AstExpression> Context::ParseTypeExpression(const String& typeString)
{
    AstIterator astIterator;

    SourceFile sourceFile(SourceLocation::eof.GetFileName(),
        typeString.Size() + 1);

    ByteBuffer temp(typeString.Size() + 1, typeString.Data());
    sourceFile.ReadIntoBuffer(temp);

    // use the lexer and parser on this file buffer
    TokenStream tokenStream(TokenStreamInfo { SourceLocation::eof.GetFileName() });

    CompilationUnit compilationUnit;

    Lexer lexer(SourceStream(&sourceFile), &tokenStream, &compilationUnit);
    lexer.Analyze();

    Parser parser(&astIterator, &tokenStream, &compilationUnit);

    RC<AstPrototypeSpecification> typeSpec =
        parser.ParsePrototypeSpecification();

    Assert(!compilationUnit.GetErrorList().HasFatalErrors(),
        "Failed to parse type expression: %s", typeString.Data());

    return typeSpec;
}

Array<RC<AstParameter>>
Context::ParseGenericParams(const String& genericParamsString)
{
    AstIterator astIterator;

    SourceFile sourceFile(SourceLocation::eof.GetFileName(),
        genericParamsString.Size() + 1);

    ByteBuffer temp(genericParamsString.Size() + 1,
        genericParamsString.Data());
    sourceFile.ReadIntoBuffer(temp);

    // use the lexer and parser on this file buffer
    TokenStream tokenStream(TokenStreamInfo { SourceLocation::eof.GetFileName() });

    CompilationUnit compilationUnit;

    Lexer lexer(SourceStream(&sourceFile), &tokenStream, &compilationUnit);
    lexer.Analyze();

    Parser parser(&astIterator, &tokenStream, &compilationUnit);

    Array<RC<AstParameter>> genericParams = parser.ParseGenericParameters();

    Assert(!compilationUnit.GetErrorList().HasFatalErrors(),
        "Failed to parse generic parameters: %s",
        genericParamsString.Data());

    return genericParams;
}

void Context::Visit(AstVisitor* visitor, CompilationUnit* compilationUnit)
{
    Mutex::Guard guard(m_mutex);

    for (GlobalDefinition& global : m_globals)
    {
        IdentifierFlagBits identifierFlags =
            IdentifierFlags::FLAG_CONST | IdentifierFlags::FLAG_NATIVE;

        RC<AstPrototypeSpecification> typeSpec =
            ParseTypeExpression(global.symbol.type.typeString)
                .Cast<AstPrototypeSpecification>();
        Assert(typeSpec != nullptr);

        RC<AstExpression> expr(new AstAsExpression(
            RC<AstNil>(new AstNil(SourceLocation::eof)),
            RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                RC<AstTypeRef>(
                    new AstTypeRef(BuiltinTypes::ANY, SourceLocation::eof)),
                SourceLocation::eof)),
            SourceLocation::eof));

        if (global.genericParamsString.HasValue())
        {
            const Array<RC<AstParameter>> genericParams =
                ParseGenericParams(*global.genericParamsString);

            if (genericParams.Any())
            {
                expr.Reset(new AstTemplateExpression(
                    expr, genericParams, typeSpec,
                    AST_TEMPLATE_EXPRESSION_FLAG_NATIVE, SourceLocation::eof));

                identifierFlags |= IdentifierFlags::FLAG_GENERIC;

                typeSpec.Reset(); // reset typeSpec so we don't double-visit it
            }
        }

        global.varDecl.Reset(
            new AstVariableDeclaration(global.symbol.name, typeSpec, expr,
                identifierFlags, SourceLocation::eof));

        visitor->GetAstIterator()->Push(global.varDecl);
    }

    for (ClassDefinition& classDefinition : m_classDefinitions)
    {
        Array<RC<AstVariableDeclaration>> members;
        members.Resize(classDefinition.members.Size());

        Array<RC<AstVariableDeclaration>> staticMembers;
        staticMembers.Resize(classDefinition.staticMembers.Size());

        FixedArray<Pair<Array<RC<AstVariableDeclaration>>*, Array<Symbol>*>, 2>
            memberArrays {
                Pair<Array<RC<AstVariableDeclaration>>*, Array<Symbol>*> {
                    &members, &classDefinition.members },
                Pair<Array<RC<AstVariableDeclaration>>*, Array<Symbol>*> {
                    &staticMembers, &classDefinition.staticMembers }
            };

        for (Pair<Array<RC<AstVariableDeclaration>>*, Array<Symbol>*>& memberArray : memberArrays)
        {
            for (SizeType i = 0; i < memberArray.second->Size(); ++i)
            {
                const Symbol& symbol = (*memberArray.second)[i];

                RC<AstPrototypeSpecification> typeSpec =
                    ParseTypeExpression(symbol.type.typeString)
                        .Cast<AstPrototypeSpecification>();
                Assert(typeSpec != nullptr);

                (*memberArray.first)[i].Reset(new AstVariableDeclaration(
                    symbol.name, typeSpec,
                    RC<AstAsExpression>(new AstAsExpression(
                        RC<AstNil>(new AstNil(SourceLocation::eof)),
                        RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                            RC<AstTypeRef>(
                                new AstTypeRef(BuiltinTypes::ANY, SourceLocation::eof)),
                            SourceLocation::eof)),
                        SourceLocation::eof)),
                    IdentifierFlags::FLAG_NATIVE, SourceLocation::eof));
            }
        }

        classDefinition.expr.Reset(
            new AstTypeExpression(classDefinition.name, nullptr, members, {},
                staticMembers, false, SourceLocation::eof));

        IdentifierFlagBits identifierFlags =
            IdentifierFlags::FLAG_CONST | IdentifierFlags::FLAG_NATIVE;

        if (classDefinition.genericParamsString.HasValue())
        {
            const Array<RC<AstParameter>> genericParams =
                ParseGenericParams(*classDefinition.genericParamsString);

            if (genericParams.Any())
            {
                classDefinition.expr.Reset(new AstTemplateExpression(
                    classDefinition.expr, genericParams, nullptr,
                    AST_TEMPLATE_EXPRESSION_FLAG_NATIVE, SourceLocation::eof));

                identifierFlags |= IdentifierFlags::FLAG_GENERIC;
            }
        }

        classDefinition.varDecl.Reset(new AstVariableDeclaration(
            classDefinition.name, nullptr, classDefinition.expr, identifierFlags,
            SourceLocation::eof));

        visitor->GetAstIterator()->Push(classDefinition.varDecl);
    }
}

void Context::BindAll(APIInstance& apiInstance, VM* vm)
{
    Mutex::Guard guard(m_mutex);

    for (const GlobalDefinition& global : m_globals)
    {
        Assert(global.varDecl != nullptr);
        Assert(global.varDecl->GetIdentifier() != nullptr);

        const int stackLocation =
            global.varDecl->GetIdentifier()->GetStackLocation();
        Assert(stackLocation != -1, "Global %s has no stack location",
            global.symbol.name.Data());

        Value value { Value::NONE, {} };

        if (global.symbol.value.Is<Value>())
        {
            value = global.symbol.value.Get<Value>();
        }
        else if (global.symbol.value.Is<NativeFunctionPtr_t>())
        {
            value = { Value::NATIVE_FUNCTION,
                { .nativeFunc = global.symbol.value.Get<NativeFunctionPtr_t>() } };
        }
        else
        {
            Assert(false);
        }

        VMState& vmState = vm->GetState();

        Assert(vmState.GetMainThread()->GetStack().STACK_SIZE > stackLocation);
        vmState.GetMainThread()->GetStack().GetData()[stackLocation] = value;
    }

    for (const ClassDefinition& classDefinition : m_classDefinitions)
    {
        Assert(classDefinition.expr != nullptr);

        Assert(classDefinition.varDecl != nullptr);
        Assert(classDefinition.varDecl->GetIdentifier() != nullptr);

        const int stackLocation =
            classDefinition.varDecl->GetIdentifier()->GetStackLocation();
        Assert(stackLocation != -1, "Class %s has no stack location",
            classDefinition.name.Data());

        // Ensure class SymbolType is registered
        SymbolTypePtr_t heldType = classDefinition.expr->GetHeldType();
        Assert(heldType != nullptr);
        heldType = heldType->GetUnaliased();

        Assert(heldType->GetId() != -1, "Class %s has no ID",
            classDefinition.name.Data());

        // Load the class object from the VM - it is stored in StaticMemory
        // at the index

        VMState& vmState = vm->GetState();

        const int index = heldType->GetId();
        Assert(vmState.m_staticMemory.staticSize > index);

        Array<Member> classObjectMembers;
        classObjectMembers.Resize(heldType->GetMembers().Size());

        for (SizeType i = 0; i < heldType->GetMembers().Size(); ++i)
        {
            const SymbolTypeMember& member = heldType->GetMembers()[i];

            auto symbolIt = classDefinition.staticMembers.FindIf(
                [&member](const Symbol& symbol)
                {
                    return symbol.name == member.name;
                });

            if (symbolIt == classDefinition.staticMembers.End())
            {
                continue;
            }

            const Symbol& symbol = *symbolIt;

            Value symbolValue { Value::NONE, {} };

            if (symbol.value.Is<Value>())
            {
                symbolValue = symbol.value.Get<Value>();
            }
            else if (symbol.value.Is<NativeFunctionPtr_t>())
            {
                symbolValue = {
                    Value::NATIVE_FUNCTION,
                    { .nativeFunc = symbol.value.Get<NativeFunctionPtr_t>() }
                };
            }
            else
            {
                Assert(false);
            }

            Memory::StrCpy(classObjectMembers[i].name, symbol.name.Data(),
                MathUtil::Min(symbol.name.Size(), 255));
            classObjectMembers[i].hash = hashFnv1(classObjectMembers[i].name);
            classObjectMembers[i].value = symbolValue;
        }

        HeapValue* classObjectHeapValue =
            vmState.HeapAlloc(vmState.GetMainThread());
        classObjectHeapValue->Assign(VMObject(
            classObjectMembers.Data(), classObjectMembers.Size(), nullptr));
        classObjectHeapValue->Mark();

        VMObject* classObjectPtr =
            classObjectHeapValue->GetPointer<VMObject>();
        Assert(classObjectPtr != nullptr);

        Array<Member> protoObjectMembers;
        protoObjectMembers.Resize(classDefinition.members.Size());

        for (SizeType i = 0; i < classDefinition.members.Size(); ++i)
        {
            const Symbol& symbol = classDefinition.members[i];

            Value symbolValue { Value::NONE, {} };

            if (symbol.value.Is<Value>())
            {
                symbolValue = symbol.value.Get<Value>();
            }
            else if (symbol.value.Is<NativeFunctionPtr_t>())
            {
                symbolValue = {
                    Value::NATIVE_FUNCTION,
                    { .nativeFunc = symbol.value.Get<NativeFunctionPtr_t>() }
                };
            }

            Memory::StrCpy(protoObjectMembers[i].name, symbol.name.Data(),
                MathUtil::Min(symbol.name.Size(), 255));
            protoObjectMembers[i].hash = hashFnv1(protoObjectMembers[i].name);
            protoObjectMembers[i].value = symbolValue;
        }

        // Add __intern member
        protoObjectMembers.PushBack(
            Member { "__intern", hashFnv1("__intern"), Value { Value::NONE, {} } });

        HeapValue* protoObjectHeapValue =
            vmState.HeapAlloc(vmState.GetMainThread());
        protoObjectHeapValue->Assign(VMObject(protoObjectMembers.Data(),
            protoObjectMembers.Size(),
            classObjectHeapValue));
        protoObjectHeapValue->Mark();

        VMObject* protoObjectPtr =
            protoObjectHeapValue->GetPointer<VMObject>();
        Assert(protoObjectPtr != nullptr);

        // Set $proto for class object
        classObjectPtr->SetMember(
            "$proto", Value { Value::HEAP_POINTER, { .ptr = protoObjectHeapValue } });

        apiInstance.classBindings.classPrototypes.Set(classDefinition.name,
            protoObjectHeapValue);
        apiInstance.classBindings.classNames.Set(classDefinition.nativeTypeId,
            classDefinition.name);

        Value value { Value::HEAP_POINTER, { .ptr = classObjectHeapValue } };

        // Set class object in static memory
        vmState.m_staticMemory[index] = value;

        // Set class object in global scope
        Assert(vmState.GetMainThread()->GetStack().STACK_SIZE > stackLocation);
        vmState.GetMainThread()->GetStack().GetData()[stackLocation] = value;

        DebugLog(LogType::Info, "Set class %s at index %d\n",
            classDefinition.name.Data(), index);
    }
}

} // namespace scriptapi2
} // namespace hyperion
