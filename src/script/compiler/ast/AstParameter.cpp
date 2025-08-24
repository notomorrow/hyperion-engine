#include <script/compiler/ast/AstParameter.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstTemplateInstantiation.hpp>
#include <script/compiler/ast/AstArgument.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/AstVisitor.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion::compiler {

AstParameter::AstParameter(
    const String& name,
    const RC<AstPrototypeSpecification>& typeSpec,
    const RC<AstExpression>& defaultParam,
    bool isVariadic,
    bool isConst,
    bool isRef,
    const SourceLocation& location)
    : AstDeclaration(name, location),
      m_typeSpec(typeSpec),
      m_defaultParam(defaultParam),
      m_isVariadic(isVariadic),
      m_isConst(isConst),
      m_isRef(isRef),
      m_isGenericParam(false)
{
}

void AstParameter::Visit(AstVisitor* visitor, Module* mod)
{
    AstDeclaration::Visit(visitor, mod);

    // params are `Any` by default
    m_symbolType = BuiltinTypes::ANY;

    SymbolTypePtr_t specifiedSymbolType;

    if (m_typeSpec != nullptr)
    {
        m_typeSpec->Visit(visitor, mod);

        if ((specifiedSymbolType = m_typeSpec->GetHeldType()))
        {
            m_symbolType = specifiedSymbolType;
        }
    }
    else
    {
        m_symbolType = BuiltinTypes::ANY;
    }

    if (m_defaultParam != nullptr)
    {
        m_defaultParam->Visit(visitor, mod);

        const SymbolTypePtr_t defaultParamType = m_defaultParam->GetExprType();
        Assert(defaultParamType != nullptr);

        if (specifiedSymbolType == nullptr)
        { // no symbol type specified; just set to the default arg type
            m_symbolType = defaultParamType;
        }
        else
        {                                                // have to check compatibility
            Assert(m_symbolType == specifiedSymbolType); // just sanity check, assigned above

            // verify types compatible
            if (!specifiedSymbolType->TypeCompatible(*defaultParamType, true))
            {
                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_arg_type_incompatible,
                    m_defaultParam->GetLocation(),
                    m_symbolType->ToString(),
                    defaultParamType->ToString()));
            }
        }
    }

    // if variadic, then change symbol type to `varargs<T>`
    if (m_isVariadic)
    {
        m_varargsTypeSpec.Reset(new AstPrototypeSpecification(
            RC<AstTemplateInstantiation>(new AstTemplateInstantiation(
                RC<AstVariable>(new AstVariable(
                    "varargs",
                    m_location)),
                { RC<AstArgument>(new AstArgument(
                    RC<AstTypeRef>(new AstTypeRef(
                        m_symbolType,
                        m_location)),
                    false,
                    false,
                    false,
                    false,
                    "T",
                    m_location)) },
                m_location)),
            m_location));

        m_varargsTypeSpec->Visit(visitor, mod);

        auto* varargsValueOf = m_varargsTypeSpec->GetDeepValueOf();
        Assert(varargsValueOf != nullptr);

        SymbolTypePtr_t heldType = varargsValueOf->GetHeldType();
        Assert(heldType != nullptr);
        heldType = heldType->GetUnaliased();

        m_symbolType = heldType;
        Assert(m_symbolType->IsVarArgsType());
    }

    if (m_identifier != nullptr)
    {
        m_identifier->SetSymbolType(m_symbolType);
        m_identifier->SetFlags(m_identifier->GetFlags() | IdentifierFlags::FLAG_ARGUMENT);

        if (m_isConst)
        {
            m_identifier->SetFlags(m_identifier->GetFlags() | IdentifierFlags::FLAG_CONST);
        }

        if (m_isRef)
        {
            m_identifier->SetFlags(m_identifier->GetFlags() | IdentifierFlags::FLAG_REF);
        }

        if (m_defaultParam != nullptr)
        {
            m_identifier->SetCurrentValue(m_defaultParam);
        }
    }
}

std::unique_ptr<Buildable> AstParameter::Build(AstVisitor* visitor, Module* mod)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    Assert(m_identifier != nullptr);

    if (m_varargsTypeSpec != nullptr)
    {
        chunk->Append(m_varargsTypeSpec->Build(visitor, mod));
    }

    // get current stack size
    const int stackLocation = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
    // set identifier stack location
    m_identifier->SetStackLocation(stackLocation);

    if (IsGenericParam())
    {
        Assert(m_defaultParam != nullptr, "Generic params must be set to a default value");

        chunk->Append(m_defaultParam->Build(visitor, mod));

        uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
        chunk->Append(BytecodeUtil::Make<StoreLocal>(rp));
    }

    visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();

    return chunk;
}

void AstParameter::Optimize(AstVisitor* visitor, Module* mod)
{
    if (m_varargsTypeSpec != nullptr)
    {
        m_varargsTypeSpec->Optimize(visitor, mod);
    }
}

RC<AstStatement> AstParameter::Clone() const
{
    return CloneImpl();
}

SymbolTypePtr_t AstParameter::GetExprType() const
{
    return m_symbolType;
}

} // namespace hyperion::compiler
