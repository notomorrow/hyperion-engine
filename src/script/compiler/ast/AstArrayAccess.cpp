#include <script/compiler/ast/AstArrayAccess.hpp>
#include <script/compiler/ast/AstMemberCallExpression.hpp>
#include <script/compiler/ast/AstCallExpression.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstHasExpression.hpp>
#include <script/compiler/ast/AstTernaryExpression.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/Module.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <core/debug/Debug.hpp>

namespace hyperion::compiler {

AstArrayAccess::AstArrayAccess(
    const RC<AstExpression>& target,
    const RC<AstExpression>& index,
    const RC<AstExpression>& rhs,
    bool operatorOverloadingEnabled,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD | ACCESS_MODE_STORE),
      m_target(target),
      m_index(index),
      m_rhs(rhs),
      m_operatorOverloadingEnabled(operatorOverloadingEnabled)
{
}

void AstArrayAccess::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(m_target != nullptr);
    Assert(m_index != nullptr);

    m_target->Visit(visitor, mod);
    m_index->Visit(visitor, mod);

    if (m_rhs != nullptr)
    {
        m_rhs->Visit(visitor, mod);
    }

    SymbolTypePtr_t targetType = m_target->GetExprType();
    Assert(targetType != nullptr);
    targetType = targetType->GetUnaliased();

    if (mod->IsInScopeOfType(ScopeType::SCOPE_TYPE_NORMAL, ScopeFunctionFlags::REF_VARIABLE_FLAG))
    {
        // TODO: implement
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_internal_error,
            m_location));
    }

    // @TODO Right hand side of array should get passed to operator[] as an argument

    if (m_operatorOverloadingEnabled)
    {
        // Treat it the same as AstBinaryExpression does - look for operator[] or operator[]=
        const String overloadFunctionName = m_rhs != nullptr
            ? "operator[]="
            : "operator[]";

        RC<AstArgumentList> argumentList(new AstArgumentList(
            { RC<AstArgument>(new AstArgument(
                CloneAstNode(m_index),
                false,
                false,
                false,
                false,
                "index",
                m_location)) },
            m_location));

        // add right hand side as argument if it exists
        if (m_rhs != nullptr)
        {
            argumentList->GetArguments().PushBack(RC<AstArgument>(new AstArgument(
                CloneAstNode(m_rhs),
                false,
                false,
                false,
                false,
                "value",
                m_location)));
        }

        RC<AstExpression> callOperatorOverloadExpr(new AstMemberCallExpression(
            overloadFunctionName,
            CloneAstNode(m_target),
            argumentList, // use right hand side as arg
            m_location));

        // check if target is an array
        if (targetType->IsProxyClass() && targetType->FindMember(overloadFunctionName))
        {
            RC<AstCallExpression> callExpr(new AstCallExpression(
                RC<AstMember>(new AstMember(
                    overloadFunctionName,
                    CloneAstNode(m_target),
                    m_location)),
                { RC<AstArgument>(new AstArgument(
                    CloneAstNode(m_index),
                    false,
                    false,
                    false,
                    false,
                    "index",
                    m_location)) },
                true,
                m_location));

            // add right hand side as argument if it exists
            if (m_rhs != nullptr)
            {
                callExpr->GetArguments().PushBack(RC<AstArgument>(new AstArgument(
                    CloneAstNode(m_rhs),
                    false,
                    false,
                    false,
                    false,
                    "value",
                    m_location)));
            }

            m_overrideExpr = std::move(callExpr);
        }
        else if (targetType->IsAnyType() || targetType->IsPlaceholderType())
        {
            auto subExpr = Clone().CastUnsafe<AstArrayAccess>();
            subExpr->SetIsOperatorOverloadingEnabled(false); // don't look for operator[] again

            m_overrideExpr.Reset(new AstTernaryExpression(
                RC<AstHasExpression>(new AstHasExpression(
                    CloneAstNode(m_target),
                    overloadFunctionName,
                    m_location)),
                callOperatorOverloadExpr,
                subExpr,
                m_location));
        }
        else if (targetType->FindPrototypeMemberDeep(overloadFunctionName))
        {
            m_overrideExpr = std::move(callOperatorOverloadExpr);
        }
        else
        {
            // Add error
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_invalid_subscript,
                m_location,
                targetType->ToString()));
        }

        if (m_overrideExpr != nullptr)
        {
            m_overrideExpr->SetAccessMode(GetAccessMode());
            m_overrideExpr->SetExpressionFlags(GetExpressionFlags());

            m_overrideExpr->Visit(visitor, mod);

            return;
        }
    }
}

UniquePtr<Buildable> AstArrayAccess::Build(AstVisitor* visitor, Module* mod)
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->Build(visitor, mod);
    }

    Assert(m_target != nullptr);
    Assert(m_index != nullptr);

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    const bool targetSideEffects = m_target->MayHaveSideEffects();
    const bool indexSideEffects = m_index->MayHaveSideEffects();

    if (m_rhs != nullptr)
    {
        chunk->Append(m_rhs->Build(visitor, mod));
    }

    uint8 rpBefore = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
    uint8 rp;
    uint8 r0, r1;

    Compiler::ExprInfo info {
        m_target.Get(),
        m_index.Get()
    };

    if (!indexSideEffects)
    {
        chunk->Append(Compiler::LoadLeftThenRight(visitor, mod, info));
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        r0 = rp - 1;
        r1 = rp;
    }
    else if (indexSideEffects && !targetSideEffects)
    {
        // load the index and store it
        chunk->Append(Compiler::LoadRightThenLeft(visitor, mod, info));
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        r0 = rp;
        r1 = rp - 1;
    }
    else
    {
        // load target, store it, then load the index
        chunk->Append(Compiler::LoadLeftAndStore(visitor, mod, info));
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        r0 = rp - 1;
        r1 = rp;
    }

    // unclaim register
    rp = visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();

    // do the operation
    if (m_accessMode == ACCESS_MODE_LOAD)
    {
        auto instr = BytecodeUtil::Make<RawOperation<>>();
        instr->opcode = LOAD_ARRAYIDX;
        instr->Accept<uint8>(rp); // destination
        instr->Accept<uint8>(r0); // source
        instr->Accept<uint8>(r1); // index

        chunk->Append(std::move(instr));
    }
    else if (m_accessMode == ACCESS_MODE_STORE)
    {
        auto instr = BytecodeUtil::Make<RawOperation<>>();
        instr->opcode = MOV_ARRAYIDX_REG;
        instr->Accept<uint8>(rp);           // destination
        instr->Accept<uint8>(r1);           // index
        instr->Accept<uint8>(rpBefore - 1); // source

        chunk->Append(std::move(instr));
    }

    return chunk;
}

void AstArrayAccess::Optimize(AstVisitor* visitor, Module* mod)
{
    if (m_overrideExpr != nullptr)
    {
        m_overrideExpr->Optimize(visitor, mod);

        return;
    }

    Assert(m_target != nullptr);
    Assert(m_index != nullptr);

    m_target->Optimize(visitor, mod);
    m_index->Optimize(visitor, mod);
}

RC<AstStatement> AstArrayAccess::Clone() const
{
    return CloneImpl();
}

Tribool AstArrayAccess::IsTrue() const
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->IsTrue();
    }

    return Tribool::Indeterminate();
}

bool AstArrayAccess::MayHaveSideEffects() const
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->MayHaveSideEffects();
    }

    return m_target->MayHaveSideEffects() || m_index->MayHaveSideEffects()
        || (m_rhs != nullptr && m_rhs->MayHaveSideEffects())
        || m_accessMode == ACCESS_MODE_STORE;
}

SymbolTypePtr_t AstArrayAccess::GetExprType() const
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->GetExprType();
    }

    if (m_rhs != nullptr)
    {
        return m_rhs->GetExprType();
    }

    Assert(m_target != nullptr);

    SymbolTypePtr_t targetType = m_target->GetExprType();
    Assert(targetType != nullptr);
    targetType = targetType->GetUnaliased();

    if (targetType->IsAnyType())
    {
        return BuiltinTypes::ANY;
    }

    if (targetType->IsPlaceholderType())
    {
        return BuiltinTypes::PLACEHOLDER;
    }

    return BuiltinTypes::ANY;
}

AstExpression* AstArrayAccess::GetTarget() const
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->GetTarget();
    }

    if (m_target != nullptr)
    {
        if (auto* nestedTarget = m_target->GetTarget())
        {
            return nestedTarget;
        }

        return m_target.Get();
    }

    return AstExpression::GetTarget();
}

bool AstArrayAccess::IsMutable() const
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->IsMutable();
    }

    Assert(m_target != nullptr);

    if (!m_target->IsMutable())
    {
        return false;
    }

    return true;
}

const AstExpression* AstArrayAccess::GetValueOf() const
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->GetValueOf();
    }

    if (m_rhs != nullptr)
    {
        return m_rhs->GetValueOf();
    }

    return AstExpression::GetValueOf();
}

const AstExpression* AstArrayAccess::GetDeepValueOf() const
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->GetDeepValueOf();
    }

    if (m_rhs != nullptr)
    {
        return m_rhs->GetDeepValueOf();
    }

    return AstExpression::GetDeepValueOf();
}

} // namespace hyperion::compiler
