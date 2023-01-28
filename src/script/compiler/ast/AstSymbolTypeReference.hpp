#ifndef AST_SYMBOL_TYPE_REFERENCE_HPP
#define AST_SYMBOL_TYPE_REFERENCE_HPP

#include <core/lib/String.hpp>
#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstTypeHolder.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <string>
#include <memory>

namespace hyperion::compiler {

class AstSymbolTypeReference : public AstExpression
{
public:
    AstSymbolTypeReference(
        const String &name,
        const SourceLocation &location
    );
    virtual ~AstSymbolTypeReference() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;

    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;
  
    virtual const AstExpression *GetValueOf() const override;
    virtual const AstExpression *GetDeepValueOf() const override;

protected:
    String m_name;

    SymbolTypePtr_t m_symbol_type;

private:
    Pointer<AstSymbolTypeReference> CloneImpl() const
    {
        return Pointer<AstSymbolTypeReference>(new AstPrototypeSpecification(
            m_name,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
