#include <script/compiler/ast/AstHashMap.hpp>
#include <script/compiler/ast/AstAsExpression.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstTemplateInstantiation.hpp>
#include <script/compiler/ast/AstArrayExpression.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>
#include <script/compiler/emit/StorageOperation.hpp>

#include <script/Instructions.hpp>
#include <script/Hasher.hpp>
#include <core/debug/Debug.hpp>

#include <core/containers/FlatSet.hpp>

#include <core/Types.hpp>

namespace hyperion::compiler {

AstHashMap::AstHashMap(
    const Array<RC<AstExpression>>& keys,
    const Array<RC<AstExpression>>& values,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_keys(keys),
      m_values(values)
{
}

void AstHashMap::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(m_keys.Size() == m_values.Size());

    m_replacedKeys.Reserve(m_keys.Size());
    m_replacedValues.Reserve(m_values.Size());

    m_exprType = BuiltinTypes::UNDEFINED;

    m_keyType = BuiltinTypes::UNDEFINED;
    m_valueType = BuiltinTypes::UNDEFINED;

    Array<Pair<RC<AstExpression>, RC<AstExpression>>> keyValuePairs;
    keyValuePairs.Reserve(m_keys.Size());

    for (SizeType i = 0; i < m_keys.Size(); ++i)
    {
        Assert(m_keys[i] != nullptr);
        Assert(m_values[i] != nullptr);

        keyValuePairs.PushBack({ m_keys[i], m_values[i] });
    }

    if (keyValuePairs.Any())
    {
        for (auto& keyValuePair : keyValuePairs)
        {
            keyValuePair.first->Visit(visitor, mod);
            keyValuePair.second->Visit(visitor, mod);

            SymbolTypeRef keyType = keyValuePair.first->GetExprType();
            SymbolTypeRef valueType = keyValuePair.second->GetExprType();

            if (keyType == nullptr || valueType == nullptr)
            {
                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_internal_error,
                    m_location));

                continue;
            }

            keyType = keyType->GetUnaliased();
            valueType = valueType->GetUnaliased();

            if (m_keyType == BuiltinTypes::UNDEFINED)
            {
                m_keyType = keyType;
            }
            else
            {
                m_keyType = SymbolType::TypePromotion(m_keyType, keyType);
            }

            if (m_valueType == BuiltinTypes::UNDEFINED)
            {
                m_valueType = valueType;
            }
            else if (!m_valueType->TypeEqual(*valueType))
            {
                m_valueType = SymbolType::TypePromotion(m_valueType, valueType);
            }

            m_replacedKeys.PushBack(CloneAstNode(keyValuePair.first));
            m_replacedValues.PushBack(CloneAstNode(keyValuePair.second));
        }
    }
    else
    {
        m_keyType = BuiltinTypes::ANY;
        m_valueType = BuiltinTypes::ANY;
    }

    // if either key or value type is undefined, set it to `Any`

    if (m_keyType == BuiltinTypes::UNDEFINED || m_valueType == BuiltinTypes::UNDEFINED)
    {
        // add error
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_cannot_determine_implicit_type,
            m_location));
    }

    for (SizeType i = 0; i < m_replacedKeys.Size(); ++i)
    {
        auto& key = m_replacedKeys[i];
        Assert(key != nullptr);

        auto& value = m_replacedValues[i];
        Assert(value != nullptr);

        auto& replacedKey = m_replacedKeys[i];
        Assert(replacedKey != nullptr);

        auto& replacedValue = m_replacedValues[i];
        Assert(replacedValue != nullptr);

        if (SymbolTypeRef keyType = key->GetExprType())
        {
            if (!keyType->TypeEqual(*m_keyType))
            {
                // Add cast
                replacedKey.Reset(new AstAsExpression(
                    replacedKey,
                    RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                        RC<AstTypeRef>(new AstTypeRef(
                            m_keyType,
                            key->GetLocation())),
                        key->GetLocation())),
                    key->GetLocation()));
            }
        }

        if (SymbolTypeRef valueType = value->GetExprType())
        {
            if (!valueType->TypeEqual(*m_valueType))
            {
                // Add cast
                replacedValue.Reset(new AstAsExpression(
                    replacedValue,
                    RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                        RC<AstTypeRef>(new AstTypeRef(
                            m_valueType,
                            value->GetLocation())),
                        value->GetLocation())),
                    value->GetLocation()));
            }
        }
    }

    // @TODO: Cache generic instance types
    m_mapTypeExpr.Reset(new AstPrototypeSpecification(
        RC<AstTemplateInstantiation>(new AstTemplateInstantiation(
            RC<AstVariable>(new AstVariable(
                "Map",
                m_location)),
            { RC<AstArgument>(new AstArgument(
                  RC<AstTypeRef>(new AstTypeRef(
                      m_keyType,
                      m_location)),
                  false,
                  false,
                  false,
                  false,
                  "K",
                  m_location)),

                RC<AstArgument>(new AstArgument(
                    RC<AstTypeRef>(new AstTypeRef(
                        m_valueType,
                        m_location)),
                    false,
                    false,
                    false,
                    false,
                    "V",
                    m_location)) },
            m_location)),
        m_location));

    m_mapTypeExpr->Visit(visitor, mod);

    SymbolTypeRef mapType = m_mapTypeExpr->GetHeldType();

    if (mapType == nullptr)
    {
        // add error

        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_internal_error,
            m_location));

        return;
    }

    mapType = mapType->GetUnaliased();

    m_exprType = mapType;

    Array<RC<AstExpression>> keyValueArrayExpressions;
    keyValueArrayExpressions.Reserve(m_replacedKeys.Size());

    for (SizeType i = 0; i < m_replacedKeys.Size(); ++i)
    {
        auto& key = m_replacedKeys[i];
        Assert(key != nullptr);

        auto& value = m_replacedValues[i];
        Assert(value != nullptr);

        Array<RC<AstExpression>> keyValuePair;
        keyValuePair.Reserve(2);

        keyValuePair.PushBack(key);
        keyValuePair.PushBack(value);

        keyValueArrayExpressions.PushBack(RC<AstArrayExpression>(new AstArrayExpression(
            keyValuePair,
            m_location)));
    }

    m_arrayExpr.Reset(new AstArrayExpression(
        keyValueArrayExpressions,
        m_location));

    m_arrayExpr->Visit(visitor, mod);
}

UniquePtr<Buildable> AstHashMap::Build(AstVisitor* visitor, Module* mod)
{
    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    Assert(m_mapTypeExpr != nullptr);
    chunk->Append(m_mapTypeExpr->Build(visitor, mod));

    // get active register
    uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    { // keep type obj in memory so we can do Map<K, V>.from(...), so push it to the stack
        auto instrPush = BytecodeUtil::Make<RawOperation<>>();
        instrPush->opcode = PUSH;
        instrPush->Accept<uint8>(rp);
        chunk->Append(std::move(instrPush));
    }

    int classStackLocation = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
    visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();

    Assert(m_arrayExpr != nullptr);
    chunk->Append(m_arrayExpr->Build(visitor, mod));

    const uint8 arrayReg = rp;

    // move array to stack
    {
        auto instrPush = BytecodeUtil::Make<RawOperation<>>();
        instrPush->opcode = PUSH;
        instrPush->Accept<uint8>(rp);
        chunk->Append(std::move(instrPush));
    }

    int arrayStackLocation = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
    // increment stack size
    visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();

    { // load class from stack back into register
        auto instrLoadOffset = BytecodeUtil::Make<StorageOperation>();
        instrLoadOffset->GetBuilder()
            .Load(rp)
            .Local()
            .ByOffset(visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize() - classStackLocation);

        chunk->Append(std::move(instrLoadOffset));
    }

    { // load member `from` from array type expr
        constexpr uint32 fromHash = hashFnv1("from");

        chunk->Append(Compiler::LoadMemberFromHash(visitor, mod, fromHash));
    }

    // Here map class and array should be the 2 items on the stack
    // so we call `from`, and the class will be the first arg, and the array will be the second arg

    { // call the `from` method
        chunk->Append(Compiler::BuildCall(
            visitor,
            mod,
            nullptr, // no target -- handled above
            uint8(2) // self, array
            ));
    }

    // decrement stack size for array type expr
    chunk->Append(BytecodeUtil::Make<PopLocal>(2));

    // pop array and type from stack
    for (uint32 i = 0; i < 2; i++)
    {
        visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
    }

    return chunk;
}

void AstHashMap::Optimize(AstVisitor* visitor, Module* mod)
{
    if (m_mapTypeExpr != nullptr)
    {
        m_mapTypeExpr->Optimize(visitor, mod);
    }

    if (m_arrayExpr != nullptr)
    {
        m_arrayExpr->Optimize(visitor, mod);
    }
}

RC<AstStatement> AstHashMap::Clone() const
{
    return CloneImpl();
}

Tribool AstHashMap::IsTrue() const
{
    return Tribool::True();
}

bool AstHashMap::MayHaveSideEffects() const
{
    // return true because we have calls to __map_new and __map_set
    return true;
}

SymbolTypeRef AstHashMap::GetExprType() const
{
    if (m_exprType == nullptr)
    {
        return BuiltinTypes::UNDEFINED;
    }

    return m_exprType;
}

} // namespace hyperion::compiler
