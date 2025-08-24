#ifndef AST_FLOAT_HPP
#define AST_FLOAT_HPP

#include <script/compiler/ast/AstConstant.hpp>

namespace hyperion::compiler {

class AstFloat : public AstConstant
{
public:
    AstFloat(float value,
        const SourceLocation& location);

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
        HashCode hc = AstConstant::GetHashCode().Add(TypeName<AstFloat>());
        hc.Add(m_value);

        return hc;
    }

private:
    float m_value;

    RC<AstFloat> CloneImpl() const
    {
        return RC<AstFloat>(new AstFloat(
            m_value,
            m_location));
    }
};

} // namespace hyperion::compiler

#endif
