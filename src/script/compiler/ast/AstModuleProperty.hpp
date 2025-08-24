#ifndef AST_MODULE_PROPERTY_HPP
#define AST_MODULE_PROPERTY_HPP

#include <script/compiler/ast/AstIdentifier.hpp>
#include <core/containers/String.hpp>

namespace hyperion::compiler {

class AstModuleProperty : public AstExpression
{
public:
    AstModuleProperty(
        const String& fieldName,
        const SourceLocation& location);
    virtual ~AstModuleProperty() override = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstModuleProperty>());
        hc.Add(m_fieldName);

        return hc;
    }

protected:
    String m_fieldName;

    // set while analyzing
    SymbolTypePtr_t m_exprType;
    RC<AstExpression> m_exprValue;

    RC<AstModuleProperty> CloneImpl() const
    {
        return RC<AstModuleProperty>(new AstModuleProperty(
            m_fieldName,
            m_location));
    }
};

} // namespace hyperion::compiler

#endif
