#include <script/compiler/ast/AstImport.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Optimizer.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/SourceFile.hpp>
#include <script/compiler/Lexer.hpp>
#include <script/compiler/Parser.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>

#include <core/utilities/StringUtil.hpp>

#include <core/Types.hpp>

#include <fstream>
#include <iostream>
#include <functional>

namespace hyperion::compiler {

AstImport::AstImport(const SourceLocation &location)
    : AstStatement(location)
{
}

void AstImport::CopyModules(
    AstVisitor *visitor,
    Module *modToCopy,
    bool updateTreeLink
)
{
    Assert(visitor != nullptr);
    Assert(modToCopy != nullptr);
    Assert(visitor->GetCompilationUnit()->GetCurrentModule() != nullptr);

    if (visitor->GetCompilationUnit()->GetCurrentModule()->LookupNestedModule(modToCopy->GetName()) != nullptr) {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_module_already_defined,
            modToCopy->GetLocation(),
            modToCopy->GetName()
        ));

        return;
    }

    // add this module to the compilation unit
    visitor->GetCompilationUnit()->m_moduleTree.Open(modToCopy);
    // open scope for module
    //mod->m_scopes.Open(Scope());

    if (updateTreeLink) {
        modToCopy->SetImportTreeLink(visitor->GetCompilationUnit()->m_moduleTree.TopNode());
    }

    // function to copy nested modules 
    std::function<void(TreeNode<Module *> *)> copyNodes = [visitor, &copyNodes, &updateTreeLink](TreeNode<Module*> *link) {
        Assert(link != nullptr);
        Assert(link->Get() != nullptr);

        for (auto *sibling : link->m_siblings) {
            Assert(sibling != nullptr);

            if (visitor->GetCompilationUnit()->GetCurrentModule()->LookupNestedModule(sibling->Get()->GetName()) != nullptr) {
                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_module_already_defined,
                    sibling->Get()->GetLocation(),
                    sibling->Get()->GetName()
                ));
            } else {
                visitor->GetCompilationUnit()->m_moduleTree.Open(sibling->Get());

                if (updateTreeLink) {
                    sibling->Get()->SetImportTreeLink(sibling);
                }

                copyNodes(sibling);

                visitor->GetCompilationUnit()->m_moduleTree.Close();
            }
        }
    };

    // copy all nested modules
    copyNodes(modToCopy->GetImportTreeLink());

    // close scope for module
    //mod->m_scopes.Close();

    // close module
    visitor->GetCompilationUnit()->m_moduleTree.Close();
}

bool AstImport::TryOpenFile(const String &path, std::ifstream &is)
{
    is.open(path.Data(), std::ios::in | std::ios::ate);
    return is.isOpen();
}

void AstImport::PerformImport(
    AstVisitor *visitor,
    Module *mod,
    const String &filepath
)
{
    Assert(visitor != nullptr);
    Assert(mod != nullptr);

    if (!mod->IsInGlobalScope()) {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_import_outside_global,
            m_location
        ));

        return;
    }

    // parse path into vector
    Array<String> pathParts = filepath.Split('\\', '/');
    // canonicalize the vector
    pathParts = StringUtil::CanonicalizePath(pathParts);
    // put it back into a string
    const String canonPath = String::Join(pathParts, '/');

    // first, check if the file has already been imported somewhere in this compilation unit
    const auto it = visitor->GetCompilationUnit()->m_importedModules.Find(canonPath);
    if (it != visitor->GetCompilationUnit()->m_importedModules.End()) {
        // imported file found, so just re-open all
        // modules that belong to the file into this scope

        // TODO: Fix issues with duplicated symbols...
        for (RC<Module> &mod : it->second) {
            AstImport::CopyModules(
                visitor,
                mod.Get(),
                false
            );
        }
    } else {
        // file hasn't been imported, so open it
        std::ifstream file;

        if (!TryOpenFile(filepath, file)) {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_could_not_open_file,
                m_location,
                filepath
            ));
        } else {
            // get number of bytes
            SizeType max = file.tellg();
            // seek to beginning
            file.seekg(0, std::ios::beg);
            // load stream into file buffer
            SourceFile sourceFile(filepath, max);

            ByteBuffer temp;
            temp.SetSize(max);

            file.read(reinterpret_cast<char *>(temp.Data()), max);

            sourceFile.ReadIntoBuffer(temp);

            // use the lexer and parser on this file buffer
            TokenStream tokenStream(TokenStreamInfo {
                filepath
            });

            Lexer lexer(SourceStream(&sourceFile), &tokenStream, visitor->GetCompilationUnit());
            lexer.Analyze();

            Parser parser(&m_astIterator, &tokenStream, visitor->GetCompilationUnit());
            parser.Parse();

            SemanticAnalyzer semanticAnalyzer(&m_astIterator, visitor->GetCompilationUnit());
            semanticAnalyzer.Analyze();
        }

        /*if (makeParentModule) {
            visitor->GetCompilationUnit()->m_moduleTree.Close();
        }*/
    }
}

std::unique_ptr<Buildable> AstImport::Build(AstVisitor *visitor, Module *mod)
{
    m_astIterator.ResetPosition();

    // compile the imported module
    Compiler compiler(&m_astIterator, visitor->GetCompilationUnit());
    return compiler.Compile();
}

void AstImport::Optimize(AstVisitor *visitor, Module *mod)
{
    m_astIterator.ResetPosition();

    // optimize the imported module
    Optimizer optimizer(&m_astIterator, visitor->GetCompilationUnit());
    optimizer.Optimize();
}

} // namespace hyperion::compiler
