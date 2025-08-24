#include <script/compiler/ast/AstTypeAlias.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>

#include <core/debug/Debug.hpp>
#include <util/UTF8.hpp>

namespace hyperion::compiler {

AstTypeAlias::AstTypeAlias(
    const String &name,
    const RC<AstPrototypeSpecification> &aliasee,
    const SourceLocation &location
) : AstStatement(location),
    m_name(name),
    m_aliasee(aliasee)
{
}

void AstTypeAlias::Visit(AstVisitor *visitor, Module *mod)
{
    Assert(visitor != nullptr && mod != nullptr);
    Assert(m_aliasee != nullptr);

    m_aliasee->Visit(visitor, mod);

    SymbolTypePtr_t aliaseeType = m_aliasee->GetHeldType();
    Assert(aliaseeType != nullptr);
    aliaseeType = aliaseeType->GetUnaliased();

    // make sure name isn't already defined
    if (mod->LookupSymbolType(m_name)) {
        // error; redeclaration of type in module
        visitor->GetCompilationUnit()->GetErrorList().AddError(
            CompilerError(
                LEVEL_ERROR,
                Msg_redefined_type,
                m_location,
                m_name
            )
        );
    } else {
        SymbolTypePtr_t aliasType = SymbolType::Alias(
            m_name, { aliaseeType }
        );

        // add it
        mod->m_scopes.Top().GetIdentifierTable().AddSymbolType(aliasType);
    }
}

std::unique_ptr<Buildable> AstTypeAlias::Build(AstVisitor *visitor, Module *mod)
{
    return nullptr;
}

void AstTypeAlias::Optimize(AstVisitor *visitor, Module *mod)
{
}

RC<AstStatement> AstTypeAlias::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion::compiler
