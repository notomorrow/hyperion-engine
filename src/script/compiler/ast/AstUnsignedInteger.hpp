#ifndef AST_UNSIGNED_INTEGER_HPP
#define AST_UNSIGNED_INTEGER_HPP

#include <script/compiler/ast/AstConstant.hpp>

#include <cstdint>

namespace hyperion::compiler {

class AstUnsignedInteger : public AstConstant
{
public:
    AstUnsignedInteger(hyperion::uint32 value, const SourceLocation& location);

    virtual std::unique_ptr<Buildable> Build(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool IsNumber() const override;
    virtual hyperion::int32 IntValue() const override;
    virtual hyperion::uint32 UnsignedValue() const override;
    virtual float FloatValue() const override;
    virtual SymbolTypePtr_t GetExprType() const override;

    virtual RC<AstConstant> HandleOperator(Operators opType, const AstConstant* right) const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstConstant::GetHashCode().Add(TypeName<AstUnsignedInteger>());
        hc.Add(m_value);

        return hc;
    }

private:
    hyperion::uint32 m_value;

    RC<AstUnsignedInteger> CloneImpl() const
    {
        return RC<AstUnsignedInteger>(new AstUnsignedInteger(
            m_value,
            m_location));
    }
};

} // namespace hyperion::compiler

#endif
