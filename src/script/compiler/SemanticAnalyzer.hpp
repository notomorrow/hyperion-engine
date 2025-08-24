#ifndef SEMANTIC_ANALYZER_HPP
#define SEMANTIC_ANALYZER_HPP

#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Identifier.hpp>
#include <script/compiler/Enums.hpp>
#include <script/compiler/ast/AstArgument.hpp>

#include <core/utilities/Optional.hpp>

#include <memory>
#include <utility>

namespace hyperion::compiler {

// forward declaration
class Module;

struct SubstitutionResult
{
    RC<AstArgument> arg;
    SizeType index = SizeType(-1);
};

struct ArgInfo
{
    bool isNamed;
    String name;
    SymbolTypePtr_t type;
};

class SemanticAnalyzer : public AstVisitor
{
public:
    struct Helpers
    {
    private:
        static SizeType FindFreeSlot(
            SizeType currentIndex,
            const FlatSet<SizeType>& usedIndices,
            const Array<GenericInstanceTypeInfo::Arg>& genericArgs,
            bool isVariadic,
            SizeType numSuppliedArgs);

        static SizeType ArgIndex(
            SizeType currentIndex,
            const ArgInfo& argInfo,
            const FlatSet<SizeType>& usedIndices,
            const Array<GenericInstanceTypeInfo::Arg>& genericArgs,
            bool isVariadic = false,
            SizeType numSuppliedArgs = -1);

    public:
        static SymbolTypePtr_t GetVarArgType(
            const Array<GenericInstanceTypeInfo::Arg>& genericArgs);

        static SymbolTypePtr_t SubstituteGenericParameters(
            AstVisitor* visitor,
            Module* mod,
            const SymbolTypePtr_t& inputType,
            const Array<GenericInstanceTypeInfo::Arg>& genericArgs,
            const Array<SubstitutionResult>& substitutionResults,
            const SourceLocation& location);

        static Optional<SymbolTypeFunctionSignature> ExtractGenericArgs(
            AstVisitor* visitor,
            Module* mod,
            const SymbolTypePtr_t& symbolType,
            const Array<RC<AstArgument>>& args,
            const SourceLocation& location,
            Array<SubstitutionResult> (*fn)(
                AstVisitor* visitor,
                Module* mod,
                const Array<GenericInstanceTypeInfo::Arg>& genericArgs,
                const Array<RC<AstArgument>>& args,
                const SourceLocation& location));

        static void CheckArgTypeCompatible(
            AstVisitor* visitor,
            const SourceLocation& location,
            const SymbolTypePtr_t& argType,
            const SymbolTypePtr_t& paramType);

        static Optional<SymbolTypeFunctionSignature> SubstituteFunctionArgs(
            AstVisitor* visitor,
            Module* mod,
            const SymbolTypePtr_t& symbolType,
            const Array<RC<AstArgument>>& args,
            const SourceLocation& location);

        static void EnsureFunctionArgCompatibility(
            AstVisitor* visitor,
            Module* mod,
            const SymbolTypePtr_t& symbolType,
            const Array<RC<AstArgument>>& args,
            const SourceLocation& location);

        static void EnsureLooseTypeAssignmentCompatibility(
            AstVisitor* visitor,
            Module* mod,
            const SymbolTypePtr_t& symbolType,
            const SymbolTypePtr_t& assignmentType,
            const SourceLocation& location);

        static void EnsureTypeAssignmentCompatibility(
            AstVisitor* visitor,
            Module* mod,
            const SymbolTypePtr_t& symbolType,
            const SymbolTypePtr_t& assignmentType,
            const SourceLocation& location);
    };

public:
    SemanticAnalyzer(AstIterator* astIterator, CompilationUnit* compilationUnit);
    SemanticAnalyzer(const SemanticAnalyzer& other);

    /** Generates the compilation unit structure from the given statement iterator */
    void Analyze(bool expectModuleDecl = true);
};

} // namespace hyperion::compiler

#endif
