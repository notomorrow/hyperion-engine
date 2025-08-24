#ifndef AST_ARRAY_ACCESS_HPP
#define AST_ARRAY_ACCESS_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/Enums.hpp>

#include <string>
#include <vector>
#include <memory>

namespace hyperion::compiler {

class AstArrayAccess : public AstExpression
{
public:
    AstArrayAccess(
        const RC<AstExpression>& target,
        const RC<AstExpression>& index,
        const RC<AstExpression>& rhs,
        bool operatorOverloadingEnabled,
        const SourceLocation& location);
    virtual ~AstArrayAccess() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    bool IsOperatorOverloadingEnabled() const
    {
        return m_operatorOverloadingEnabled;
    }

    void SetIsOperatorOverloadingEnabled(bool operatorOverloadingEnabled)
    {
        m_operatorOverloadingEnabled = operatorOverloadingEnabled;
    }

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;
    virtual AstExpression* GetTarget() const override;
    virtual bool IsMutable() const override;

    virtual const AstExpression* GetValueOf() const override;
    virtual const AstExpression* GetDeepValueOf() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstArrayAccess>());
        hc.Add(m_target ? m_target->GetHashCode() : HashCode());
        hc.Add(m_index ? m_index->GetHashCode() : HashCode());
        hc.Add(m_rhs ? m_rhs->GetHashCode() : HashCode());
        hc.Add(m_operatorOverloadingEnabled);

        return hc;
    }

private:
    RC<AstExpression> m_target;
    RC<AstExpression> m_index;
    RC<AstExpression> m_rhs;
    bool m_operatorOverloadingEnabled;

    // set while analyzing
    RC<AstExpression> m_overrideExpr;

    RC<AstArrayAccess> CloneImpl() const
    {
        return RC<AstArrayAccess>(new AstArrayAccess(
            CloneAstNode(m_target),
            CloneAstNode(m_index),
            CloneAstNode(m_rhs),
            m_operatorOverloadingEnabled,
            m_location));
    }
};

} // namespace hyperion::compiler

#endif
