#ifndef AST_TYPE_EXPRESSION_HPP
#define AST_TYPE_EXPRESSION_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>

#include <string>
#include <memory>
#include <vector>

namespace hyperion::compiler {

class AstTypeExpression : public AstExpression {
public:
    AstTypeExpression(
        const std::string &name,
        const std::shared_ptr<AstPrototypeSpecification> &base_specification,
        const std::vector<std::shared_ptr<AstVariableDeclaration>> &members,
        const std::vector<std::shared_ptr<AstVariableDeclaration>> &static_members,
        bool is_proxy_class,
        const SourceLocation &location
    );

    AstTypeExpression(
        const std::string &name,
        const std::shared_ptr<AstPrototypeSpecification> &base_specification,
        const std::vector<std::shared_ptr<AstVariableDeclaration>> &members,
        const std::vector<std::shared_ptr<AstVariableDeclaration>> &static_members,
        const SymbolTypePtr_t &enum_underlying_type,
        bool is_proxy_class,
        const SourceLocation &location
    );

    virtual ~AstTypeExpression() = default;

    /** enable setting to that variable declarations can change the type name */
    void SetName(const std::string &name)
        { m_name = name; }

    const std::vector<std::shared_ptr<AstVariableDeclaration>> &GetMembers() const
        { return m_members; }

    int GetNumMembers() const
        { return m_num_members; }

    bool IsEnum() const
        { return m_enum_underlying_type != nullptr; }

    bool IsProxyClass() const
        { return m_is_proxy_class; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;

    virtual bool IsLiteral() const override;
    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;
    virtual const AstExpression *GetValueOf() const override;
    virtual const AstExpression *GetDeepValueOf() const override;

    virtual const std::string &GetName() const override;

protected:
    std::string m_name;
    std::shared_ptr<AstPrototypeSpecification> m_base_specification;
    std::vector<std::shared_ptr<AstVariableDeclaration>> m_members;
    std::vector<std::shared_ptr<AstVariableDeclaration>> m_static_members;
    SymbolTypePtr_t m_enum_underlying_type;
    bool m_is_proxy_class;
    int m_num_members;

    SymbolTypePtr_t m_symbol_type;

    std::shared_ptr<AstTypeObject> m_expr;
    std::vector<std::shared_ptr<AstVariableDeclaration>> m_outside_members;

    Pointer<AstTypeExpression> CloneImpl() const
    {
        return Pointer<AstTypeExpression>(new AstTypeExpression(
            m_name,
            CloneAstNode(m_base_specification),
            CloneAllAstNodes(m_members),
            CloneAllAstNodes(m_static_members),
            m_enum_underlying_type,
            m_is_proxy_class,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
