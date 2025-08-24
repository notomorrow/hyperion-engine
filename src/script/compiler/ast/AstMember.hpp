#ifndef AST_MEMBER_HPP
#define AST_MEMBER_HPP

#include <script/compiler/ast/AstIdentifier.hpp>
#include <core/containers/String.hpp>

namespace hyperion::compiler {

class AstMember : public AstExpression
{
public:
    AstMember(
        const String &fieldName,
        const RC<AstExpression> &target,
        const SourceLocation &location
    );
    virtual ~AstMember() = default;
    
    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;

    virtual SymbolTypePtr_t GetExprType() const override;
    virtual SymbolTypePtr_t GetHeldType() const override;
  
    virtual const AstExpression *GetValueOf() const override;
    virtual const AstExpression *GetDeepValueOf() const override;
    virtual AstExpression *GetTarget() const override;
    virtual bool IsMutable() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstMember>());
        hc.Add(m_fieldName);
        hc.Add(m_target ? m_target->GetHashCode() : HashCode());

        return hc;
    }

protected:
    String              m_fieldName;
    RC<AstExpression>   m_target;

    // set while analyzing
    SymbolTypePtr_t     m_symbolType;
    SymbolTypePtr_t     m_targetType;
    SymbolTypePtr_t     m_heldType;
    RC<AstExpression>   m_proxyExpr;
    RC<AstExpression>   m_overrideExpr;
    uint32                m_foundIndex;
    bool                m_enableGenericMemberSubstitution;

    RC<AstMember> CloneImpl() const
    {
        return RC<AstMember>(new AstMember(
            m_fieldName,
            CloneAstNode(m_target),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
