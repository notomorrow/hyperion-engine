#ifndef AST_STRING_HPP
#define AST_STRING_HPP

#include <script/compiler/ast/AstConstant.hpp>
#include <core/containers/String.hpp>

#include <string>

namespace hyperion::compiler {

class AstString : public AstConstant
{
public:
    AstString(const String &value, const SourceLocation &location);

    const String &GetValue() const { return m_value; }

    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool IsNumber() const override;
    virtual hyperion::int32 IntValue() const override;
    virtual float FloatValue() const override;
    virtual SymbolTypePtr_t GetExprType() const override;

    virtual RC<AstConstant> HandleOperator(Operators opType, const AstConstant *right) const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstConstant::GetHashCode().Add(TypeName<AstString>());
        hc.Add(m_value);

        return hc;
    }

private:
    String  m_value;

    // set while compiling
    int     m_staticId;

    RC<AstString> CloneImpl() const
    {
        return RC<AstString>(new AstString(
            m_value,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
