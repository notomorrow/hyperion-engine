#pragma once

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstVariable.hpp>

namespace hyperion::compiler {

class AstTypeRef;

class AstTypeObject : public AstExpression
{
public:
    AstTypeObject(
        const SymbolTypeRef& symbolType,
        const SymbolTypeRef& baseSymbolType, // base here is usually CLASS_TYPE - it is not the same as polymorphic base
        const SourceLocation& location);

    AstTypeObject(
        const SymbolTypeRef& symbolType,
        const SymbolTypeRef& baseSymbolType,
        const SymbolTypeRef& enumUnderlyingType,
        bool isProxyClass,
        const SourceLocation& location);

    virtual ~AstTypeObject() = default;

    bool IsEnum() const
    {
        return m_enumUnderlyingType != nullptr;
    }

    const SymbolTypeRef& GetEnumUnderlyingType() const
    {
        return m_enumUnderlyingType;
    }

    bool IsProxyClass() const
    {
        return m_isProxyClass;
    }

    bool IsVisited() const
    {
        return m_isVisited;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;

    virtual SymbolTypeRef GetExprType() const override;
    virtual SymbolTypeRef GetHeldType() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstTypeObject>());
        hc.Add(m_symbolType ? m_symbolType->GetHashCode() : HashCode());
        hc.Add(m_baseSymbolType ? m_baseSymbolType->GetHashCode() : HashCode());
        hc.Add(m_enumUnderlyingType ? m_enumUnderlyingType->GetHashCode() : HashCode());
        hc.Add(m_isProxyClass);

        return hc;
    }

private:
    SymbolTypeRef m_symbolType;
    SymbolTypeRef m_baseSymbolType;
    SymbolTypeRef m_enumUnderlyingType;
    bool m_isProxyClass;

    // set while analyzing
    RC<AstTypeRef> m_baseTypeRef;
    Array<RC<AstExpression>> m_memberExpressions;
    bool m_isVisited;

    RC<AstTypeObject> CloneImpl() const
    {
        return RC<AstTypeObject>(new AstTypeObject(
            m_symbolType,
            m_baseSymbolType,
            m_enumUnderlyingType,
            m_isProxyClass,
            m_location));
    }
};

} // namespace hyperion::compiler

