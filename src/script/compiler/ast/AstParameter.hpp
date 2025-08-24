#ifndef AST_PARAMETER_HPP
#define AST_PARAMETER_HPP

#include <script/compiler/ast/AstDeclaration.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstExpression.hpp>
#include <core/containers/String.hpp>

namespace hyperion::compiler {

class AstParameter : public AstDeclaration
{
public:
    AstParameter(
        const String& name,
        const RC<AstPrototypeSpecification>& typeSpec,
        const RC<AstExpression>& defaultParam,
        bool isVariadic,
        bool isConst,
        bool isRef,
        const SourceLocation& location);

    virtual ~AstParameter() override = default;

    const RC<AstExpression>& GetDefaultValue() const
    {
        return m_defaultParam;
    }

    void SetDefaultValue(const RC<AstExpression>& defaultParam)
    {
        m_defaultParam = defaultParam;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    bool IsVariadic() const
    {
        return m_isVariadic;
    }
    bool IsConst() const
    {
        return m_isConst;
    }
    bool IsRef() const
    {
        return m_isRef;
    }

    bool IsGenericParam() const
    {
        return m_isGenericParam;
    }
    void SetIsGenericParam(bool isGenericParam)
    {
        m_isGenericParam = isGenericParam;
    }

    // used by AstTemplateExpression
    const RC<AstPrototypeSpecification>& GetPrototypeSpecification() const
    {
        return m_typeSpec;
    }

    void SetPrototypeSpecification(const RC<AstPrototypeSpecification>& typeSpec)
    {
        m_typeSpec = typeSpec;
    }

    SymbolTypeRef GetExprType() const;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstDeclaration::GetHashCode().Add(TypeName<AstParameter>());
        hc.Add(m_typeSpec ? m_typeSpec->GetHashCode() : HashCode());
        hc.Add(m_defaultParam ? m_defaultParam->GetHashCode() : HashCode());
        hc.Add(m_isVariadic);
        hc.Add(m_isConst);
        hc.Add(m_isRef);
        hc.Add(m_isGenericParam);

        return hc;
    }

private:
    RC<AstPrototypeSpecification> m_typeSpec;
    RC<AstExpression> m_defaultParam;
    bool m_isVariadic;
    bool m_isConst;
    bool m_isRef;
    bool m_isGenericParam;

    // Set while analyzing
    SymbolTypeRef m_symbolType;
    RC<AstExpression> m_varargsTypeSpec;

    RC<AstParameter> CloneImpl() const
    {
        return RC<AstParameter>(new AstParameter(
            m_name,
            CloneAstNode(m_typeSpec),
            CloneAstNode(m_defaultParam),
            m_isVariadic,
            m_isConst,
            m_isRef,
            m_location));
    }
};

} // namespace hyperion::compiler

#endif
