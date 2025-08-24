#include <script/compiler/ast/AstNodeBuilder.hpp>
#include <script/compiler/ast/AstCallExpression.hpp>
#include <script/compiler/ast/AstVariable.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion::compiler {

ModuleBuilder AstNodeBuilder::Module(const String& name)
{
    return ModuleBuilder(name);
}

ModuleBuilder::ModuleBuilder(
    const String& name)
    : m_name(name),
      m_parent(nullptr)
{
}

ModuleBuilder::ModuleBuilder(
    const String& name,
    ModuleBuilder* parent)
    : m_name(name),
      m_parent(parent)
{
}

ModuleBuilder ModuleBuilder::Module(const String& name)
{
    return ModuleBuilder(name, this);
}

FunctionBuilder ModuleBuilder::Function(const String& name)
{
    return FunctionBuilder(name, this);
}

RC<AstModuleAccess> ModuleBuilder::Build(const RC<AstExpression>& expr)
{
    if (m_parent != nullptr)
    {
        return RC<AstModuleAccess>(new AstModuleAccess(
            m_name,
            m_parent->Build(expr),
            SourceLocation::eof));
    }
    else
    {
        return RC<AstModuleAccess>(new AstModuleAccess(
            m_name,
            expr,
            SourceLocation::eof));
    }
}

FunctionBuilder::FunctionBuilder(
    const String& name)
    : m_name(name),
      m_parent(nullptr)
{
}

FunctionBuilder::FunctionBuilder(
    const String& name,
    ModuleBuilder* parent)
    : m_name(name),
      m_parent(parent)
{
}

RC<AstExpression> FunctionBuilder::Call(const Array<RC<AstArgument>>& args)
{
    RC<AstCallExpression> call(new AstCallExpression(
        RC<AstVariable>(new AstVariable(
            m_name,
            SourceLocation::eof)),
        args,
        false,
        SourceLocation::eof));

    if (m_parent != nullptr)
    {
        return m_parent->Build(call);
    }
    else
    {
        return call;
    }
}

} // namespace hyperion::compiler
