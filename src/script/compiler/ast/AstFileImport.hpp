#ifndef AST_LOCAL_IMPORT_HPP
#define AST_LOCAL_IMPORT_HPP

#include <script/compiler/ast/AstImport.hpp>
#include <core/lib/String.hpp>

#include <string>

namespace hyperion::compiler {

class AstFileImport : public AstImport {
public:
    AstFileImport(
        const String &path,
        const SourceLocation &location
    );

    virtual ~AstFileImport() override = default;

    const String &GetPath() const { return m_path; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

protected:
    String m_path;

    RC<AstFileImport> CloneImpl() const
    {
        return RC<AstFileImport>(new AstFileImport(
            m_path,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
