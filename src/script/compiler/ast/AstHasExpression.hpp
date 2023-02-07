#ifndef AST_HAS_EXPRESSION_HPP
#define AST_HAS_EXPRESSION_HPP

#include <script/compiler/ast/AstExpression.hpp>

#include <string>

namespace hyperion::compiler {

class AstHasExpression : public AstExpression
{
public:
    AstHasExpression(
        const RC<AstStatement> &target,
        const std::string &field_name,
        const SourceLocation &location
    );
    virtual ~AstHasExpression() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;

protected:
    RC<AstStatement> m_target;
    std::string m_field_name;

    // set while analyzing
    Tribool m_has_member;
    // is it a check if an expression has the member,
    // or is it a check if a type has a member?
    bool m_is_expr;
    bool m_has_side_effects;

private:
    RC<AstHasExpression> CloneImpl() const
    {
        return RC<AstHasExpression>(new AstHasExpression(
            CloneAstNode(m_target),
            m_field_name,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
