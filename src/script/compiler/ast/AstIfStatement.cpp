#include <script/compiler/ast/AstIfStatement.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/Keywords.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>

#include <cstdio>

namespace hyperion::compiler {

AstIfStatement::AstIfStatement(
    const RC<AstExpression>& conditional,
    const RC<AstBlock>& block,
    const RC<AstBlock>& elseBlock,
    const SourceLocation& location)
    : AstStatement(location),
      m_conditional(conditional),
      m_block(block),
      m_elseBlock(elseBlock)
{
}

void AstIfStatement::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(m_conditional != nullptr);
    Assert(m_block != nullptr);

    // visit the conditional
    m_conditional->Visit(visitor, mod);
    // visit the body
    m_block->Visit(visitor, mod);
    // visit the else-block (if it exists)
    if (m_elseBlock != nullptr)
    {
        m_elseBlock->Visit(visitor, mod);
    }
}

std::unique_ptr<Buildable> AstIfStatement::Build(AstVisitor* visitor, Module* mod)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    int conditionIsTrue = m_conditional->IsTrue();

    if (conditionIsTrue == -1)
    {
        // the condition cannot be determined at compile time
        chunk->Append(Compiler::CreateConditional(
            visitor,
            mod,
            m_conditional.Get(),
            m_block.Get(),
            m_elseBlock.Get()));
    }
    else if (conditionIsTrue)
    {
        // the condition has been determined to be true
        if (m_conditional->MayHaveSideEffects())
        {
            // if there is a possibility of side effects,
            // build the conditional into the binary
            chunk->Append(m_conditional->Build(visitor, mod));
        }
        // enter the block
        chunk->Append(m_block->Build(visitor, mod));
        // do not accept the else-block
    }
    else
    {
        // the condition has been determined to be false
        if (m_conditional->MayHaveSideEffects())
        {
            // if there is a possibility of side effects,
            // build the conditional into the binary
            chunk->Append(m_conditional->Build(visitor, mod));
        }
        // only visit the else-block (if it exists)
        if (m_elseBlock != nullptr)
        {
            chunk->Append(m_elseBlock->Build(visitor, mod));
        }
    }

    return chunk;
}

void AstIfStatement::Optimize(AstVisitor* visitor, Module* mod)
{
    // optimize the conditional
    m_conditional->Optimize(visitor, mod);
    // optimize the body
    m_block->Optimize(visitor, mod);
    // optimize the else-block (if it exists)
    if (m_elseBlock != nullptr)
    {
        m_elseBlock->Optimize(visitor, mod);
    }
}

RC<AstStatement> AstIfStatement::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion::compiler
