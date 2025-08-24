#ifndef AST_MEMBER_CALL_EXPRESSION_HPP
#define AST_MEMBER_CALL_EXPRESSION_HPP

#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstArgumentList.hpp>
#include <core/containers/String.hpp>

namespace hyperion::compiler {

class AstMemberCallExpression : public AstMember {
public:
    AstMemberCallExpression(
        const String &fieldName,
        const RC<AstExpression> &target,
        const RC<AstArgumentList> &arguments,
        const SourceLocation &location
    );
    virtual ~AstMemberCallExpression() override = default;
    
    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;
  
    virtual const AstExpression *GetValueOf() const override;
    virtual const AstExpression *GetDeepValueOf() const override;
    virtual AstExpression *GetTarget() const override;

protected:
    RC<AstArgumentList>     m_arguments;

    // set while analyzing
    Array<RC<AstArgument>>  m_substitutedArgs;
    SymbolTypePtr_t         m_returnType;

    RC<AstMemberCallExpression> CloneImpl() const
    {
        return RC<AstMemberCallExpression>(new AstMemberCallExpression(
            m_fieldName,
            CloneAstNode(m_target),
            CloneAstNode(m_arguments),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
