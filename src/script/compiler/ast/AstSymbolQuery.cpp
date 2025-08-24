#include <script/compiler/ast/AstSymbolQuery.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/ast/AstFalse.hpp>
#include <script/compiler/ast/AstTrue.hpp>

#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/HypScript.hpp>
#include <script/SourceFile.hpp>

#include <core/debug/Debug.hpp>

#include <core/io/ByteReader.hpp>
#include <core/memory/UniquePtr.hpp>

namespace hyperion::compiler {

AstSymbolQuery::AstSymbolQuery(
    const String& commandName,
    const RC<AstExpression>& expr,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_commandName(commandName),
      m_expr(expr)
{
}

void AstSymbolQuery::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(m_expr != nullptr);
    m_expr->Visit(visitor, mod);

    m_symbolType = BuiltinTypes::UNDEFINED;

    if (m_commandName == "inspectType")
    {
        auto* valueOf = m_expr->GetDeepValueOf();
        Assert(valueOf != nullptr);

        SymbolTypeRef heldType = valueOf->GetHeldType();
        if (heldType == nullptr)
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_custom_error,
                m_location,
                "No held type found for expression"));

            return;
        }

        heldType = heldType->GetUnaliased();

        m_resultValue = RC<AstString>::Construct(heldType->ToString(true), m_location);
    }
    else if (m_commandName == "log")
    {
        const auto* valueOf = m_expr->GetDeepValueOf();

        if (valueOf)
        {
            const auto* stringValue = dynamic_cast<const AstString*>(valueOf);

            if (stringValue)
            {
                DebugLog(LogType::Info, "$meta::log(): %s\n", stringValue->GetValue().Data());
            }
            else
            {
                DebugLog(LogType::Warn, "$meta::log(): Not a constant string\n");
            }
        }
        else
        {
            DebugLog(LogType::Warn, "$meta::log(): No value found for expression\n");
        }
    }
    else if (m_commandName == "fields")
    {
        auto* valueOf = m_expr->GetDeepValueOf();
        Assert(valueOf != nullptr);

        SymbolTypeRef heldType = valueOf->GetHeldType();
        if (heldType == nullptr)
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_custom_error,
                m_location,
                "No held type found for expression"));

            return;
        }

        heldType = heldType->GetUnaliased();

        String fieldNames;

        for (SizeType i = 0; i < heldType->GetMembers().Size(); ++i)
        {
            const SymbolTypeMember& member = heldType->GetMembers()[i];

            if (i > 0)
            {
                fieldNames += ",";
            }

            fieldNames += member.name;
        }

        m_resultValue = RC<AstString>::Construct(fieldNames, m_location);

        // Array<RC<AstExpression>> fieldNames;

        // for (const SymbolTypeMember &member : heldType->GetMembers()) {
        //     fieldNames.PushBack(RC<AstString>::Construct(member.name, m_location));
        // }

        // m_resultValue = RC<AstArrayExpression>(new AstArrayExpression(
        //     fieldNames,
        //     m_location
        // ));

        m_resultValue->Visit(visitor, mod);
    }
    else if (m_commandName == "compiles")
    {
        const auto* valueOf = m_expr->GetDeepValueOf();
        const AstString* stringValue = nullptr;

        if (valueOf)
        {
            stringValue = dynamic_cast<const AstString*>(valueOf);
        }

        if (!stringValue)
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_internal_error,
                m_location));

            return;
        }

        String value = stringValue->GetValue();

        ByteBuffer byteBuffer;
        byteBuffer.SetData(value.Size(), value.Data());

        SourceFile sourceFile(value, value.Size());
        sourceFile.ReadIntoBuffer(byteBuffer);

        UniquePtr<HypScript> script(new HypScript(sourceFile));

        scriptapi2::Context context;

        if (script->Compile(context))
        {
            script->Bake();

            m_resultValue = RC<AstTrue>(new AstTrue(m_location));
        }
        else
        {
            m_resultValue = RC<AstFalse>(new AstFalse(m_location));
        }
    }
    else
    {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_invalid_symbol_query,
            m_location,
            m_commandName));
    }
}

UniquePtr<Buildable> AstSymbolQuery::Build(AstVisitor* visitor, Module* mod)
{
    if (m_resultValue != nullptr)
    {
        return m_resultValue->Build(visitor, mod);
    }

    return nullptr;
}

void AstSymbolQuery::Optimize(AstVisitor* visitor, Module* mod)
{
}

RC<AstStatement> AstSymbolQuery::Clone() const
{
    return CloneImpl();
}

Tribool AstSymbolQuery::IsTrue() const
{
    return Tribool::Indeterminate();
}

bool AstSymbolQuery::MayHaveSideEffects() const
{
    return false;
}

SymbolTypeRef AstSymbolQuery::GetExprType() const
{
    if (m_resultValue != nullptr)
    {
        return m_resultValue->GetExprType();
    }

    return BuiltinTypes::UNDEFINED;
}

const AstExpression* AstSymbolQuery::GetValueOf() const
{
    if (m_resultValue != nullptr)
    {
        return m_resultValue.Get();
    }

    return this;
}

} // namespace hyperion::compiler
