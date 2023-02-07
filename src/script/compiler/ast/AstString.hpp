#ifndef AST_STRING_HPP
#define AST_STRING_HPP

#include <script/compiler/ast/AstConstant.hpp>

#include <string>

namespace hyperion::compiler {

class AstString : public AstConstant {
public:
    AstString(const std::string &value, const SourceLocation &location);

    const std::string &GetValue() const { return m_value; }

    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool IsNumber() const override;
    virtual hyperion::Int32 IntValue() const override;
    virtual hyperion::Float32 FloatValue() const override;
    virtual SymbolTypePtr_t GetExprType() const override;

    virtual RC<AstConstant> HandleOperator(Operators op_type, const AstConstant *right) const override;

private:
    std::string m_value;

    // set while compiling
    int m_static_id;

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
