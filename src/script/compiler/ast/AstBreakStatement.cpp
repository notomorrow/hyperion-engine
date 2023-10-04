#include <script/compiler/ast/AstBreakStatement.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/Keywords.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/Debug.hpp>

namespace hyperion::compiler {

AstBreakStatement::AstBreakStatement(const SourceLocation &location)
    : AstStatement(location),
      m_num_pops(0)
{
}

void AstBreakStatement::Visit(AstVisitor *visitor, Module *mod)
{
    bool in_loop = false;

    TreeNode<Scope> *top = mod->m_scopes.TopNode();

    while (top != nullptr) {
        if (top->m_value.GetScopeType() == SCOPE_TYPE_LOOP) {
            in_loop = true;

            break;
        }

        m_num_pops += top->m_value.GetIdentifierTable().CountUsedVariables();
        top = top->m_parent;
    }

    if (!in_loop) {
        // error; 'return' not allowed outside of a function
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_break_outside_loop,
            m_location
        ));
    }
}

std::unique_ptr<Buildable> AstBreakStatement::Build(AstVisitor *visitor, Module *mod)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    visitor->GetCompilationUnit()->GetInstructionStream().

    const auto end_label_id = chunk->NewLabel();

    chunk->Append(Compiler::PopStack(visitor, m_num_pops));

    return nullptr;
}

void AstBreakStatement::Optimize(AstVisitor *visitor, Module *mod)
{
}

RC<AstStatement> AstBreakStatement::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion::compiler
