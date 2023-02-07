#include <script/compiler/ast/AstNodeBuilder.hpp>
#include <script/compiler/ast/AstCallExpression.hpp>
#include <script/compiler/ast/AstVariable.hpp>

#include <system/Debug.hpp>

namespace hyperion::compiler {

ModuleBuilder AstNodeBuilder::Module(const std::string &name)
{
    return ModuleBuilder(name);
}

ModuleBuilder::ModuleBuilder(
    const std::string &name
) : m_name(name),
    m_parent(nullptr)
{
}

ModuleBuilder::ModuleBuilder(
    const std::string &name,
    ModuleBuilder *parent
) : m_name(name),
    m_parent(parent)
{
}

ModuleBuilder ModuleBuilder::Module(const std::string &name)
{
    return ModuleBuilder(name, this);
}

FunctionBuilder ModuleBuilder::Function(const std::string &name)
{
    return FunctionBuilder(name, this);
}

RC<AstModuleAccess> ModuleBuilder::Build(const RC<AstExpression> &expr)
{
    if (m_parent != nullptr) {
        return RC<AstModuleAccess>(new AstModuleAccess(
            m_name,
            m_parent->Build(expr),
            SourceLocation::eof
        ));
    } else {
        return RC<AstModuleAccess>(new AstModuleAccess(
            m_name,
            expr,
            SourceLocation::eof
        ));
    }
}


FunctionBuilder::FunctionBuilder(
    const std::string &name
) : m_name(name),
    m_parent(nullptr)
{
}

FunctionBuilder::FunctionBuilder(
    const std::string &name,
    ModuleBuilder *parent
) : m_name(name),
    m_parent(parent)
{
}

RC<AstExpression> FunctionBuilder::Call(const Array<RC<AstArgument>> &args)
{
    RC<AstCallExpression> call(new AstCallExpression(
        RC<AstVariable>(new AstVariable(
            m_name,
            SourceLocation::eof
        )),
        args,
        false,
        SourceLocation::eof
    ));

    if (m_parent != nullptr) {
        return m_parent->Build(call);
    } else {
        return call;
    }
}

} // namespace hyperion::compiler
