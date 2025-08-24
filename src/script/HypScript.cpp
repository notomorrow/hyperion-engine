#include <script/HypScript.hpp>

#include <script/compiler/Module.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/Optimizer.hpp>
#include <script/vm/BytecodeStream.hpp>
#include <script/compiler/ast/AstModuleDeclaration.hpp>
#include <script/compiler/emit/Instruction.hpp>
#include <script/compiler/Lexer.hpp>
#include <script/compiler/Parser.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/dis/DecompilationUnit.hpp>
#include <script/compiler/emit/codegen/CodeGenerator.hpp>
#include <script/compiler/dis/DecompilationUnit.hpp>
#include <script/compiler/builtins/Builtins.hpp>

#include <script/ScriptBindings.hpp>
#include <script/ScriptApi.hpp>

#include <script/vm/VM.hpp>

namespace hyperion {

HypScript::HypScript(const SourceFile& sourceFile)
    : m_apiInstance(sourceFile),
      m_vm(m_apiInstance),
      m_sourceFile(sourceFile)
{
}

HypScript::~HypScript() = default;

bool HypScript::Compile(scriptapi2::Context& context)
{
    if (!m_sourceFile.IsValid())
    {
        DebugLog(
            LogType::Error,
            "Source file not valid\n");

        return false;
    }

    SourceStream sourceStream(&m_sourceFile);

    TokenStream tokenStream(TokenStreamInfo {
        m_sourceFile.GetFilePath() });

    Lexer lex(sourceStream, &tokenStream, &m_compilationUnit);
    lex.Analyze();

    AstIterator astIterator;

    SemanticAnalyzer semanticAnalyzer(&astIterator, &m_compilationUnit);

    m_compilationUnit.GetBuiltins().Visit(&semanticAnalyzer);

    // Generate script bindings into our Context for our C++ classes
    g_scriptBindings.GenerateAll(context);

    context.Visit(&semanticAnalyzer, &m_compilationUnit);

    Parser parser(&astIterator, &tokenStream, &m_compilationUnit);
    parser.Parse();

    semanticAnalyzer.Analyze();

    m_errors = m_compilationUnit.GetErrorList();
    m_errors.WriteOutput(std::cout);

    if (!m_errors.HasFatalErrors())
    {
        // only optimize if there were no errors
        // before this point
        astIterator.ResetPosition();

        Optimizer optimizer(&astIterator, &m_compilationUnit);
        optimizer.Optimize();

        // compile into bytecode instructions
        astIterator.ResetPosition();

        Compiler compiler(&astIterator, &m_compilationUnit);

        // if (auto builtinsResult = builtins.Build(&m_compilationUnit)) {
        //     m_bytecodeChunk.Append(std::move(builtinsResult));
        // } else {
        //     DebugLog(
        //         LogType::Error,
        //         "Failed to add builtins to script\n"
        //     );

        //     return false;
        // }

        if (auto compileResult = compiler.Compile())
        {
            m_bytecodeChunk.Append(std::move(compileResult));
        }
        else
        {
            DebugLog(
                LogType::Error,
                "Failed to compile source file\n");

            return false;
        }

        return true;
    }

    return false;
}

InstructionStream HypScript::Decompile(std::ostream* os) const
{
    Assert(IsCompiled() && IsBaked());

    BytecodeStream bytecodeStream(m_bakedBytes.Data(), m_bakedBytes.Size());

    return DecompilationUnit().Decompile(bytecodeStream, os);
}

void HypScript::Bake()
{
    Assert(IsCompiled());

    BuildParams buildParams {};

    Bake(buildParams);
}

void HypScript::Bake(BuildParams& buildParams)
{
    Assert(IsCompiled());

    CodeGenerator codeGenerator(buildParams);
    codeGenerator.Visit(&m_bytecodeChunk);
    codeGenerator.Bake();

    m_bakedBytes = codeGenerator.GetInternalByteStream().GetData();

    m_bs = BytecodeStream(m_bakedBytes.Data(), m_bakedBytes.Size());
}

void HypScript::Run(scriptapi2::Context& context)
{
    Assert(IsCompiled() && IsBaked());

    // bad things will happen if we don't set the VM
    m_apiInstance.SetVM(&m_vm);

    context.BindAll(m_apiInstance, &m_vm);
    m_vm.Execute(&m_bs);
}

void HypScript::CallFunctionArgV(const FunctionHandle& handle, Value* args, ArgCount numArgs)
{
    Assert(IsCompiled() && IsBaked());

    auto* mainThread = m_vm.GetState().GetMainThread();

    if (numArgs != 0)
    {
        Assert(args != nullptr);

        for (ArgCount i = 0; i < numArgs; i++)
        {
            mainThread->m_stack.Push(args[i]);
        }
    }

    m_vm.InvokeNow(
        &m_bs,
        handle._inner,
        numArgs);

    if (numArgs != 0)
    {
        mainThread->m_stack.Pop(numArgs);
    }
}

} // namespace hyperion
