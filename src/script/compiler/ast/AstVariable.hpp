#ifndef AST_VARIABLE_HPP
#define AST_VARIABLE_HPP

#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <core/containers/String.hpp>

namespace hyperion::compiler {

class AstTypeRef;

class AstVariable : public AstIdentifier
{
public:
    AstVariable(const String& name, const SourceLocation& location);
    virtual ~AstVariable() override = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual bool IsLiteral() const override;
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypeRef GetExprType() const override;
    virtual bool IsMutable() const override;

    virtual const AstExpression* GetValueOf() const override;
    virtual const AstExpression* GetDeepValueOf() const override;

private:
    // set while analyzing
    // used to get locals from outer function in a closure
    RC<AstMember> m_closureMemberAccess;
    RC<AstMember> m_selfMemberAccess;
    RC<AstTypeRef> m_typeRef;
    RC<AstExpression> m_inlineValue;
    bool m_shouldInline;
    bool m_isInRefAssignment;
    bool m_isInConstAssignment;
    bool m_isVisited = false;

    RC<AstVariable> CloneImpl() const
    {
        return RC<AstVariable>(new AstVariable(
            m_name,
            m_location));
    }
};

} // namespace hyperion::compiler

#endif
