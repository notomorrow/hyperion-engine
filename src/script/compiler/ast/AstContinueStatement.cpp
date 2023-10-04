#include <script/compiler/ast/AstContinueStatement.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/Keywords.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/Debug.hpp>

namespace hyperion::compiler {

AstContinueStatement::AstContinueStatement(const SourceLocation &location)
    : AstStatement(location)
{
}

void AstContinueStatement::Visit(AstVisitor *visitor, Module *mod)
{
}

std::unique_ptr<Buildable> AstContinueStatement::Build(AstVisitor *visitor, Module *mod)
{
    return nullptr;
}

void AstContinueStatement::Optimize(AstVisitor *visitor, Module *mod)
{
}

RC<AstStatement> AstContinueStatement::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion::compiler
