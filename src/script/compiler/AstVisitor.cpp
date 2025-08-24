#include <script/compiler/AstVisitor.hpp>

namespace hyperion::compiler {

AstVisitor::AstVisitor(AstIterator *astIterator,
    CompilationUnit *compilationUnit)
    : m_astIterator(astIterator),
      m_compilationUnit(compilationUnit)
{
}

bool AstVisitor::AddErrorIfFalse(bool expr, const CompilerError &error)
{
    if (!expr) {
        m_compilationUnit->GetErrorList().AddError(error);
        return false;
    }
    return true;
}

} // namespace hyperion::compiler
