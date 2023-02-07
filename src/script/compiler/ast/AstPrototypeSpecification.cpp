#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstHasExpression.hpp>
#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Compiler.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <system/Debug.hpp>
#include <util/UTF8.hpp>
#include <script/Hasher.hpp>

namespace hyperion::compiler {

AstPrototypeSpecification::AstPrototypeSpecification(
    const RC<AstExpression> &proto,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
    m_proto(proto)
{
}

void AstPrototypeSpecification::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    AssertThrow(m_proto != nullptr);
    m_proto->Visit(visitor, mod);

    const AstExpression *value_of = m_proto->GetDeepValueOf(); // GetDeepValueOf() returns the non-wrapped generic AstTypeObject*
    const AstTypeObject *type_obj = nullptr;
    const AstIdentifier *identifier = nullptr;

    const SymbolTypePtr_t expr_type = value_of->GetExprType();
    AssertThrow(expr_type != nullptr);

    SymbolTypePtr_t unaliased;

    if (!expr_type->IsAlias() || !(unaliased = expr_type->GetUnaliased())) {
        unaliased = expr_type;
    }

    if (value_of != nullptr) {
        if (!(type_obj = dynamic_cast<const AstTypeObject *>(value_of))) {
            if ((identifier = dynamic_cast<const AstIdentifier *>(value_of))) {
                type_obj = identifier->ExtractTypeObject();
            }
        }
    }

    if (!type_obj) {
        type_obj = unaliased->GetTypeObject().Get();
    }

    m_symbol_type = BuiltinTypes::UNDEFINED;
    m_prototype_type = BuiltinTypes::UNDEFINED;

    if (unaliased->IsAnyType()) {
        // it is a dynamic type
        m_symbol_type = BuiltinTypes::ANY;
        m_prototype_type = BuiltinTypes::ANY_TYPE;
        m_default_value = BuiltinTypes::ANY->GetDefaultValue();

        return;
    }

    SymbolTypePtr_t found_symbol_type;

    if (type_obj != nullptr) {
        if (type_obj->IsEnum()) {
            found_symbol_type = type_obj->GetEnumUnderlyingType();
        } else {
            found_symbol_type = type_obj->GetHeldType();
        }

        AssertThrow(found_symbol_type != nullptr);
    } else if (identifier != nullptr) {
        found_symbol_type = mod->LookupSymbolType(identifier->GetName());
    }

    if (found_symbol_type != nullptr) {
        found_symbol_type = found_symbol_type->GetUnaliased();
        AssertThrow(found_symbol_type != nullptr);

        if (FindPrototypeType(found_symbol_type)) {
            m_symbol_type = found_symbol_type;
        } else {
            if (found_symbol_type != expr_type && found_symbol_type != nullptr) {
                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_type_missing_prototype,
                    m_location,
                    expr_type->GetName() + " (expanded: " + found_symbol_type->GetName() + ")"
                ));
            } else {
                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_type_missing_prototype,
                    m_location,
                    expr_type->GetName()
                ));
            }
        }
    } else {
        // m_symbol_type = constructor_type;
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_WARN,
            Msg_not_a_constant_type,
            m_location,
            expr_type->GetName()
        ));
    }

    AssertThrow(m_symbol_type != nullptr);
    AssertThrow(m_prototype_type != nullptr);
}

std::unique_ptr<Buildable> AstPrototypeSpecification::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_proto != nullptr);
    return m_proto->Build(visitor, mod);
}

void AstPrototypeSpecification::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_proto != nullptr);
    m_proto->Optimize(visitor, mod);
}

bool AstPrototypeSpecification::FindPrototypeType(const SymbolTypePtr_t &symbol_type)
{
    if (symbol_type->GetTypeClass() == TYPE_BUILTIN) {
        m_prototype_type = symbol_type;
        m_default_value = symbol_type->GetDefaultValue();

        return true;
    }

    SymbolMember_t proto_member;

    if (symbol_type->FindMember("$proto", proto_member)) {
        m_prototype_type = std::get<1>(proto_member);

        if (m_prototype_type->GetTypeClass() == TYPE_BUILTIN) {
            m_default_value = std::get<2>(proto_member);
        }

        return true;
    }

    return false;
}

RC<AstStatement> AstPrototypeSpecification::Clone() const
{
    return CloneImpl();
}

Tribool AstPrototypeSpecification::IsTrue() const
{
    return Tribool::True();
}

bool AstPrototypeSpecification::MayHaveSideEffects() const
{
    return false;
}

SymbolTypePtr_t AstPrototypeSpecification::GetExprType() const
{
    if (m_proto != nullptr) {
        return m_proto->GetExprType();
    }

    return nullptr;
}

const AstExpression *AstPrototypeSpecification::GetValueOf() const
{
    if (m_proto != nullptr) {
        return m_proto->GetValueOf();
    }

    return AstExpression::GetValueOf();
}

const AstExpression *AstPrototypeSpecification::GetDeepValueOf() const
{
    if (m_proto != nullptr) {
        return m_proto->GetDeepValueOf();
    }

    return AstExpression::GetDeepValueOf();
}

} // namespace hyperion::compiler
