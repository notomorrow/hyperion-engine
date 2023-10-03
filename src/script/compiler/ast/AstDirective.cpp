#include <script/compiler/ast/AstDirective.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/CompilerError.hpp>

#include <system/Debug.hpp>

namespace hyperion::compiler {

AstDirective::AstDirective(
    const String &key,
    const Array<String> &args,
    const SourceLocation &location
) : AstStatement(location),
    m_key(key),
    m_args(args)
{
}

void AstDirective::Visit(AstVisitor *visitor, Module *mod)
{
    if (m_key == "importpath") {
        // library names should be supplied in arguments as all strings
        if (m_args.Empty()) {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_custom_error,
                m_location,
                String("'importpath' directive requires path names to be provided (e.g '#importpath \"../path\" \"../other/path\"')")
            ));
        } else {
            // split current file path and remove file name
            const String current_dir = String::Join(m_location.GetFileName().Split('/', '\\').Slice(0, -2), '/') + "/";
            
            for (const String &path_arg : m_args) {
                const String scan_path = current_dir + path_arg;

                // create relative path
                DebugLog(LogType::Info, "[Script] add scan path %s\n", scan_path.Data());

                mod->AddScanPath(scan_path);
            }
        }
    } else {
        CompilerError error(
            LEVEL_ERROR,
            Msg_unknown_directive,
            m_location,
            m_key
        );

        visitor->GetCompilationUnit()->GetErrorList().AddError(error);
    }
}

std::unique_ptr<Buildable> AstDirective::Build(AstVisitor *visitor, Module *mod)
{
    return nullptr;
}

void AstDirective::Optimize(AstVisitor *visitor, Module *mod)
{

}

RC<AstStatement> AstDirective::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion::compiler
