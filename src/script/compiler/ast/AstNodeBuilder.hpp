#ifndef AST_NODE_BUILDER_HPP
#define AST_NODE_BUILDER_HPP

#include <script/compiler/ast/AstFunctionDefinition.hpp>
#include <script/compiler/ast/AstArgument.hpp>
#include <script/compiler/ast/AstModuleAccess.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <util/NonOwningPtr.hpp>
#include <core/lib/String.hpp>

#include <string>
#include <memory>
#include <vector>
#include <utility>

namespace hyperion::compiler {

class AstVisitor;
class Module;
class ModuleBuilder;
class FunctionBuilder;

class AstNodeBuilder
{
public:
    ModuleBuilder Module(const String &name);
};

class ModuleBuilder
{
public:
    ModuleBuilder(
        const String &name
    );

    ModuleBuilder(
        const String &name,
        ModuleBuilder *parent
    );

    const String &GetName() const
        { return m_name; }

    ModuleBuilder Module(const String &name);
    FunctionBuilder Function(const String &name);

    RC<AstModuleAccess> Build(const RC<AstExpression> &expr);

private:
    String          m_name;
    ModuleBuilder   *m_parent;
};

class FunctionBuilder
{
public:
    FunctionBuilder(
        const String &name
    );

    FunctionBuilder(
        const String &name,
        ModuleBuilder *parent
    );

    RC<AstExpression> Call(const Array<RC<AstArgument>> &args);

    const String &GetName() const { return m_name; }

private:
    String          m_name;
    ModuleBuilder   *m_parent;
};

} // namespace hyperion::compiler

#endif
