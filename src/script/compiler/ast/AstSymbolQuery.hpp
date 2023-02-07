#ifndef AST_SYMBOL_QUERY
#define AST_SYMBOL_QUERY

#include <string>

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstString.hpp>
#include <script/compiler/ast/AstArrayExpression.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

namespace hyperion::compiler {

class AstSymbolQuery : public AstExpression {
public:
    AstSymbolQuery(
      const std::string &command_name,
      const RC<AstExpression> &expr,
      const SourceLocation &location);
    virtual ~AstSymbolQuery() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;
    virtual const AstExpression *GetValueOf() const override;

private:
    std::string m_command_name;
    RC<AstExpression> m_expr;

    // set while analyzing
    SymbolTypePtr_t m_symbol_type;
    RC<AstString> m_string_result_value;
    RC<AstArrayExpression> m_array_result_value;

    RC<AstSymbolQuery> CloneImpl() const
    {
        return RC<AstSymbolQuery>(new AstSymbolQuery(
            m_command_name,
            CloneAstNode(m_expr),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
