#ifndef AST_VARIABLE_DECLARATION_HPP
#define AST_VARIABLE_DECLARATION_HPP

#include <script/compiler/ast/AstDeclaration.hpp>
#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstParameter.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/Identifier.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <memory>

namespace hyperion::compiler {

class AstVariableDeclaration : public AstDeclaration
{
public:
    AstVariableDeclaration(
        const std::string &name,
        const RC<AstPrototypeSpecification> &proto,
        const RC<AstExpression> &assignment,
        const Array<RC<AstParameter>> &template_params,
        IdentifierFlagBits flags,
        const SourceLocation &location
    );
    virtual ~AstVariableDeclaration() = default;

    const RC<AstExpression> &GetAssignment() const
        { return m_assignment; }

    const RC<AstExpression> &GetRealAssignment() const
        { return m_real_assignment; }

    const RC<AstPrototypeSpecification> &GetPrototypeSpecification() const
        { return m_proto; }

    void SetPrototypeSpecification(const RC<AstPrototypeSpecification> &proto)
        { m_proto = proto; }

    bool IsConst() const { return m_flags & IdentifierFlags::FLAG_CONST; }
    bool IsRef() const { return m_flags & IdentifierFlags::FLAG_REF; }
    bool IsGeneric() const { return m_flags & IdentifierFlags::FLAG_GENERIC; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

protected:
    RC<AstPrototypeSpecification> m_proto;
    RC<AstExpression> m_assignment;
    Array<RC<AstParameter>> m_template_params;
    IdentifierFlagBits m_flags;

    // set while analyzing
    RC<AstExpression> m_real_assignment;

    SymbolTypeWeakPtr_t m_symbol_type;

    RC<AstVariableDeclaration> CloneImpl() const
    {
        return RC<AstVariableDeclaration>(new AstVariableDeclaration(
            m_name,
            CloneAstNode(m_proto),
            CloneAstNode(m_assignment),
            CloneAllAstNodes(m_template_params),
            m_flags,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
