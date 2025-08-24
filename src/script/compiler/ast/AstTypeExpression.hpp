#ifndef AST_TYPE_EXPRESSION_HPP
#define AST_TYPE_EXPRESSION_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>

#include <string>
#include <memory>
#include <vector>

namespace hyperion::compiler {

class AstTypeExpression : public AstExpression
{
public:
    AstTypeExpression(
        const String &name,
        const RC<AstPrototypeSpecification> &baseSpecification,
        const Array<RC<AstVariableDeclaration>> &dataMembers,
        const Array<RC<AstVariableDeclaration>> &functionMembers,
        const Array<RC<AstVariableDeclaration>> &staticMembers,
        bool isProxyClass,
        const SourceLocation &location
    );

    AstTypeExpression(
        const String &name,
        const RC<AstPrototypeSpecification> &baseSpecification,
        const Array<RC<AstVariableDeclaration>> &dataMembers,
        const Array<RC<AstVariableDeclaration>> &functionMembers,
        const Array<RC<AstVariableDeclaration>> &staticMembers,
        const SymbolTypePtr_t &enumUnderlyingType,
        bool isProxyClass,
        const SourceLocation &location
    );

    virtual ~AstTypeExpression() override = default;

    /** enable setting to that variable declarations can change the type name */
    void SetName(const String &name)
        { m_name = name; }

    Array<RC<AstVariableDeclaration>> &GetDataMembers()
        { return m_dataMembers; }

    const Array<RC<AstVariableDeclaration>> &GetDataMembers() const
        { return m_dataMembers; }

    Array<RC<AstVariableDeclaration>> &GetFunctionMembers()
        { return m_functionMembers; }

    const Array<RC<AstVariableDeclaration>> &GetFunctionMembers() const
        { return m_functionMembers; }

    Array<RC<AstVariableDeclaration>> &GetStaticMembers()
        { return m_staticMembers; }

    const Array<RC<AstVariableDeclaration>> &GetStaticMembers() const
        { return m_functionMembers; }

    bool IsEnum() const
        { return m_enumUnderlyingType != nullptr; }

    bool IsProxyClass() const
        { return m_isProxyClass; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;

    virtual bool IsLiteral() const override;
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    
    virtual SymbolTypePtr_t GetExprType() const override;
    virtual SymbolTypePtr_t GetHeldType() const override;

    virtual const AstExpression *GetValueOf() const override;
    virtual const AstExpression *GetDeepValueOf() const override;

    virtual const String &GetName() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstTypeExpression>());
        hc.Add(m_name);
        hc.Add(m_baseSpecification ? m_baseSpecification->GetHashCode() : HashCode());

        for (auto &member : m_dataMembers) {
            hc.Add(member ? member->GetHashCode() : HashCode());
        }

        for (auto &member : m_functionMembers) {
            hc.Add(member ? member->GetHashCode() : HashCode());
        }

        for (auto &member : m_staticMembers) {
            hc.Add(member ? member->GetHashCode() : HashCode());
        }

        if (m_enumUnderlyingType != nullptr) {
            hc.Add(m_enumUnderlyingType->GetHashCode());
        }

        hc.Add(m_isProxyClass);

        return hc;
    }

protected:
    String                              m_name;
    RC<AstPrototypeSpecification>       m_baseSpecification;
    Array<RC<AstVariableDeclaration>>   m_dataMembers;
    Array<RC<AstVariableDeclaration>>   m_functionMembers;
    Array<RC<AstVariableDeclaration>>   m_staticMembers;
    SymbolTypePtr_t                     m_enumUnderlyingType;
    bool                                m_isProxyClass;

    SymbolTypePtr_t                     m_symbolType;

    RC<AstTypeObject>                   m_typeObject;
    RC<AstTypeObject>                   m_prototypeExpr;
    RC<AstTypeRef>                      m_typeRef;
    Array<RC<AstVariableDeclaration>>   m_outsideMembers;
    Array<RC<AstVariableDeclaration>>   m_combinedMembers;
    bool                                m_isUninstantiatedGeneric;
    bool                                m_isVisited;

    RC<AstTypeExpression> CloneImpl() const
    {
        return RC<AstTypeExpression>(new AstTypeExpression(
            m_name,
            CloneAstNode(m_baseSpecification),
            CloneAllAstNodes(m_dataMembers),
            CloneAllAstNodes(m_functionMembers),
            CloneAllAstNodes(m_staticMembers),
            m_enumUnderlyingType,
            m_isProxyClass,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
