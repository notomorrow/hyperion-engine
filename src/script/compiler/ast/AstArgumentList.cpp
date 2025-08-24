#include <script/compiler/ast/AstArgumentList.hpp>
#include <script/compiler/AstVisitor.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <core/debug/Debug.hpp>

namespace hyperion::compiler {

AstArgumentList::AstArgumentList(
    const Array<RC<AstArgument>>& args,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_args(args)
{
}

void AstArgumentList::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(visitor != nullptr);
    Assert(mod != nullptr);

    for (const RC<AstArgument>& arg : m_args)
    {
        Assert(arg != nullptr);
        arg->Visit(visitor, mod);
    }
}

UniquePtr<Buildable> AstArgumentList::Build(AstVisitor* visitor, Module* mod)
{
    Assert(visitor != nullptr);
    Assert(mod != nullptr);

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    for (const RC<AstArgument>& arg : m_args)
    {
        Assert(arg != nullptr);
        chunk->Append(arg->Build(visitor, mod));
    }

    return chunk;
}

void AstArgumentList::Optimize(AstVisitor* visitor, Module* mod)
{
    Assert(visitor != nullptr);
    Assert(mod != nullptr);

    for (const RC<AstArgument>& arg : m_args)
    {
        Assert(arg != nullptr);
        arg->Optimize(visitor, mod);
    }
}

RC<AstStatement> AstArgumentList::Clone() const
{
    return CloneImpl();
}

Tribool AstArgumentList::IsTrue() const
{
    return Tribool::True();
}

bool AstArgumentList::MayHaveSideEffects() const
{
    for (const RC<AstArgument>& arg : m_args)
    {
        Assert(arg != nullptr);

        if (arg->MayHaveSideEffects())
        {
            return true;
        }
    }

    return false;
}

SymbolTypeRef AstArgumentList::GetExprType() const
{
    return BuiltinTypes::ANY;
}

} // namespace hyperion::compiler
