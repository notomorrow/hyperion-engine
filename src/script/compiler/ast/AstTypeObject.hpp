#ifndef AST_TYPE_OBJECT_HPP
#define AST_TYPE_OBJECT_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstVariable.hpp>

namespace hyperion::compiler {

class AstTypeRef;

class AstTypeObject : public AstExpression
{
public:
    AstTypeObject(
        const SymbolTypePtr_t& symbolType,
        const SymbolTypePtr_t& baseSymbolType, // base here is usually CLASS_TYPE - it is not the same as polymorphic base
        const SourceLocation& location);

    AstTypeObject(
        const SymbolTypePtr_t& symbolType,
        const SymbolTypePtr_t& baseSymbolType,
        const SymbolTypePtr_t& enumUnderlyingType,
        bool isProxyClass,
        const SourceLocation& location);

    virtual ~AstTypeObject() = default;

    bool IsEnum() const
    {
        return m_enumUnderlyingType != nullptr;
    }

    const SymbolTypePtr_t& GetEnumUnderlyingType() const
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

    virtual SymbolTypePtr_t GetExprType() const override;
    virtual SymbolTypePtr_t GetHeldType() const override;

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
    SymbolTypePtr_t m_symbolType;
    SymbolTypePtr_t m_baseSymbolType;
    SymbolTypePtr_t m_enumUnderlyingType;
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

#endif
