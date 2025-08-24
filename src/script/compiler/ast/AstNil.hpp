#ifndef AST_NIL_HPP
#define AST_NIL_HPP

#include <script/compiler/ast/AstConstant.hpp>

namespace hyperion::compiler {

class AstNil : public AstConstant
{
public:
    AstNil(const SourceLocation& location);

    virtual std::unique_ptr<Buildable> Build(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool IsNumber() const override;
    virtual hyperion::int32 IntValue() const override;
    virtual float FloatValue() const override;
    virtual SymbolTypePtr_t GetExprType() const override;

    virtual RC<AstConstant> HandleOperator(Operators opType, const AstConstant* right) const override;

private:
    RC<AstNil> CloneImpl() const
    {
        return RC<AstNil>(new AstNil(
            m_location));
    }
};

} // namespace hyperion::compiler

#endif
