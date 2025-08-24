#ifndef AST_HAS_EXPRESSION_HPP
#define AST_HAS_EXPRESSION_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <core/containers/String.hpp>

#include <string>

namespace hyperion::compiler {

class AstHasExpression : public AstExpression
{
public:
    AstHasExpression(
        const RC<AstStatement>& target,
        const String& fieldName,
        const SourceLocation& location);
    virtual ~AstHasExpression() override = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypeRef GetExprType() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstHasExpression>());
        hc.Add(m_target ? m_target->GetHashCode() : HashCode());
        hc.Add(m_fieldName);

        return hc;
    }

protected:
    RC<AstStatement> m_target;
    String m_fieldName;

    // set while analyzing
    Tribool m_hasMember;
    // is it a check if an expression has the member,
    // or is it a check if a type has a member?
    bool m_isExpr;
    bool m_hasSideEffects;

private:
    RC<AstHasExpression> CloneImpl() const
    {
        return RC<AstHasExpression>(new AstHasExpression(
            CloneAstNode(m_target),
            m_fieldName,
            m_location));
    }
};

} // namespace hyperion::compiler

#endif
