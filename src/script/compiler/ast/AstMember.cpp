#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstCallExpression.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/ast/AstModuleAccess.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/Keywords.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <core/debug/Debug.hpp>
#include <script/Hasher.hpp>

#include <iostream>

namespace hyperion::compiler {

AstMember::AstMember(
    const String& fieldName,
    const RC<AstExpression>& target,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD | ACCESS_MODE_STORE),
      m_fieldName(fieldName),
      m_target(target),
      m_symbolType(BuiltinTypes::UNDEFINED),
      m_foundIndex(~0u),
      m_enableGenericMemberSubstitution(true)
{
}

void AstMember::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(m_target != nullptr);
    m_target->Visit(visitor, mod);

    bool isProxyClass = false;

    m_accessOptions = m_target->GetAccessOptions();

    m_targetType = m_target->GetExprType();
    Assert(m_targetType != nullptr);
    m_targetType = m_targetType->GetUnaliased();

    if (mod->IsInScopeOfType(ScopeType::SCOPE_TYPE_NORMAL, ScopeFunctionFlags::REF_VARIABLE_FLAG))
    {
        // TODO: implement
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_internal_error,
            m_location));
    }

    const SymbolTypePtr_t originalType = m_targetType;

    // start looking at the target type,
    // iterate through base type
    SymbolTypePtr_t fieldType = nullptr;
    SymbolTypeMember member;

    for (uint32 depth = 0; fieldType == nullptr && m_targetType != nullptr; depth++)
    {
        Assert(m_targetType != nullptr);
        m_targetType = m_targetType->GetUnaliased();

        if (m_targetType->IsAnyType())
        {
            fieldType = BuiltinTypes::ANY;

            break;
        }

        if (m_targetType->IsPlaceholderType() || m_targetType->IsGenericParameter())
        {
            fieldType = BuiltinTypes::PLACEHOLDER;

            break;
        }

        isProxyClass = m_targetType->IsProxyClass();

        if (isProxyClass)
        {
            // load the type by name
            m_proxyExpr.Reset(new AstPrototypeSpecification(
                RC<AstTypeRef>(new AstTypeRef(
                    m_targetType,
                    m_location)),
                m_location));

            m_proxyExpr->Visit(visitor, mod);

            // if it is a proxy class,
            // convert thing.DoThing()
            // to ThingProxy.DoThing(thing)
            if (m_targetType->FindMember(m_fieldName, member, m_foundIndex))
            {
                fieldType = member.type;
            }

            break;
        }

        { // Check for members on the object's prototype
            uint32 fieldIndex = ~0u;

            if (m_targetType->FindPrototypeMember(m_fieldName, member, fieldIndex))
            {
                // only set m_foundIndex if found in first level.
                // for members from base objects,
                // we load based on hash.
                if (depth == 0)
                {
                    m_foundIndex = fieldIndex;
                }

                fieldType = member.type;

                break;
            }
        }

        if (auto base = m_targetType->GetBaseType())
        {
            m_targetType = base->GetUnaliased();
        }
        else
        {
            break;
        }
    }

    Assert(m_targetType != nullptr);

    // Look for members on the object itself (static members)
    if (fieldType == nullptr)
    {
        const AstExpression* valueOf = m_target->GetDeepValueOf();
        Assert(valueOf != nullptr);

        if (SymbolTypePtr_t heldType = valueOf->GetHeldType())
        {
            if (heldType->IsAnyType())
            {
                fieldType = BuiltinTypes::ANY;
            }
            else if (heldType->IsPlaceholderType() || heldType->IsGenericParameter())
            {
                fieldType = BuiltinTypes::PLACEHOLDER;
            }
            else
            {
                uint32 fieldIndex = ~0u;
                uint32 depth = 0;

                if (heldType->FindMemberDeep(m_fieldName, member, fieldIndex, depth))
                {
                    // only set m_foundIndex if found in first level.
                    // for members from base objects,
                    // we load based on hash.
                    if (depth == 0)
                    {
                        m_foundIndex = fieldIndex;
                    }

                    fieldType = member.type;
                }
            }
        }
    }

    if (fieldType != nullptr)
    {
        fieldType = fieldType->GetUnaliased();
        Assert(fieldType != nullptr);

        if (m_enableGenericMemberSubstitution && fieldType->IsGenericExpressionType())
        {
            // @FIXME
            // Cloning the member will unfortunately will break closure captures used
            // in a member function, but it's the best we can do for now.
            // it also will cause too many clones to be made, making a larger bytecode chunk.
            m_overrideExpr = CloneAstNode(member.expr);
            Assert(m_overrideExpr != nullptr, "member %s is generic but has no value", m_fieldName.Data());

            m_overrideExpr->Visit(visitor, mod);

            m_symbolType = m_overrideExpr->GetExprType();
        }
        else
        {
            m_symbolType = fieldType;
        }

        Assert(m_symbolType != nullptr);

        m_symbolType = m_symbolType->GetUnaliased();
    }
    else
    {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_not_a_data_member,
            m_location,
            m_fieldName,
            originalType->ToString()));
    }
}

std::unique_ptr<Buildable> AstMember::Build(AstVisitor* visitor, Module* mod)
{
    if (m_overrideExpr != nullptr)
    {
        m_overrideExpr->SetAccessMode(m_accessMode);
        return m_overrideExpr->Build(visitor, mod);
    }

    // if (m_proxyExpr != nullptr) {
    //     m_proxyExpr->SetAccessMode(m_accessMode);
    //     return m_proxyExpr->Build(visitor, mod);
    // }

    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    if (m_proxyExpr != nullptr)
    {
        chunk->Append(m_proxyExpr->Build(visitor, mod));
    }
    else
    {
        Assert(m_target != nullptr);
        chunk->Append(m_target->Build(visitor, mod));
    }

    if (m_foundIndex == ~0u)
    {
        // no exact index of member found, have to load from hash.
        const uint32 hash = hashFnv1(m_fieldName.Data());

        switch (m_accessMode)
        {
        case ACCESS_MODE_LOAD:
            chunk->Append(Compiler::LoadMemberFromHash(visitor, mod, hash));
            break;
        case ACCESS_MODE_STORE:
            chunk->Append(Compiler::StoreMemberFromHash(visitor, mod, hash));
            break;
        default:
            Assert(false, "unknown access mode");
        }
    }
    else
    {
        switch (m_accessMode)
        {
        case ACCESS_MODE_LOAD:
            // just load the data member.
            chunk->Append(Compiler::LoadMemberAtIndex(
                visitor,
                mod,
                m_foundIndex));
            break;
        case ACCESS_MODE_STORE:
            // we are in storing mode, so store to LAST item in the member expr.
            chunk->Append(Compiler::StoreMemberAtIndex(
                visitor,
                mod,
                m_foundIndex));
            break;
        default:
            Assert(false, "unknown access mode");
        }
    }

    switch (m_accessMode)
    {
    case ACCESS_MODE_LOAD:
        chunk->Append(BytecodeUtil::Make<Comment>("Load member " + m_fieldName));
        break;
    case ACCESS_MODE_STORE:
        chunk->Append(BytecodeUtil::Make<Comment>("Store member " + m_fieldName));
        break;
    }

    return chunk;
}

void AstMember::Optimize(AstVisitor* visitor, Module* mod)
{
    if (m_overrideExpr != nullptr)
    {
        m_overrideExpr->Optimize(visitor, mod);

        return;
    }

    if (m_proxyExpr != nullptr)
    {
        m_proxyExpr->Optimize(visitor, mod);

        // return;
    }

    Assert(m_target != nullptr);

    m_target->Optimize(visitor, mod);

    // TODO: check if the member being accessed is constant and can
    // be optimized
}

RC<AstStatement> AstMember::Clone() const
{
    return CloneImpl();
}

Tribool AstMember::IsTrue() const
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->IsTrue();
    }

    // if (m_proxyExpr != nullptr) {
    //     return m_proxyExpr->IsTrue();
    // }

    return Tribool::Indeterminate();
}

bool AstMember::MayHaveSideEffects() const
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->MayHaveSideEffects();
    }

    if (m_proxyExpr != nullptr && m_proxyExpr->MayHaveSideEffects())
    {
        return true;
    }

    Assert(m_target != nullptr);

    return m_target->MayHaveSideEffects() || m_accessMode == ACCESS_MODE_STORE;
}

SymbolTypePtr_t AstMember::GetExprType() const
{
    return m_symbolType;
    // if (m_overrideExpr != nullptr) {
    //     return m_overrideExpr->GetExprType();
    // }
    // // if (m_proxyExpr != nullptr) {
    // //     return m_proxyExpr->GetExprType();
    // // }

    // Assert(m_symbolType != nullptr);

    // return m_symbolType;
}

SymbolTypePtr_t AstMember::GetHeldType() const
{
    if (m_heldType != nullptr)
    {
        return m_heldType;
    }

    return AstExpression::GetHeldType();
}

const AstExpression* AstMember::GetValueOf() const
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->GetValueOf();
    }

    // if (m_proxyExpr != nullptr) {
    //     return m_proxyExpr->GetValueOf();
    // }

    return AstExpression::GetValueOf();
}

const AstExpression* AstMember::GetDeepValueOf() const
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->GetDeepValueOf();
    }

    // if (m_proxyExpr != nullptr) {
    //     return m_proxyExpr->GetDeepValueOf();
    // }

    return AstExpression::GetDeepValueOf();
}

AstExpression* AstMember::GetTarget() const
{
    // if (m_overrideExpr != nullptr) {
    //     return m_overrideExpr->GetTarget();
    // }

    // if (m_proxyExpr != nullptr) {
    //     return m_proxyExpr->GetTarget();
    // }

    if (m_target != nullptr)
    {
        return m_target.Get();
    }

    return AstExpression::GetTarget();
}

bool AstMember::IsMutable() const
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->IsMutable();
    }

    if (m_proxyExpr != nullptr && !m_proxyExpr->IsMutable())
    {
        return false;
    }

    Assert(m_target != nullptr);
    Assert(m_symbolType != nullptr);

    if (!m_target->IsMutable())
    {
        return false;
    }

    return true;
}

} // namespace hyperion::compiler
