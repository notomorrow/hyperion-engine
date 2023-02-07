#ifndef AST_SYNTAX_DEFINITION_HPP
#define AST_SYNTAX_DEFINITION_HPP

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/ast/AstString.hpp>

#include <memory>
#include <vector>

namespace hyperion::compiler {

class AstSyntaxDefinition : public AstStatement {
public:
    AstSyntaxDefinition(
        const RC<AstString> &syntax_string,
        const RC<AstString> &transform_string,
        const SourceLocation &location);
    virtual ~AstSyntaxDefinition() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

protected:
    RC<AstString> m_syntax_string;
    RC<AstString> m_transform_string;

    RC<AstSyntaxDefinition> CloneImpl() const
    {
        return RC<AstSyntaxDefinition>(new AstSyntaxDefinition(
            CloneAstNode(m_syntax_string),
            CloneAstNode(m_transform_string),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
