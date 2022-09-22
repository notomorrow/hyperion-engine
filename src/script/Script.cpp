#include "Script.hpp"

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

#include <Engine.hpp>

namespace hyperion::v2 {

Script::Script(const SourceFile &source_file)
    : EngineComponentBase(),
      m_source_file(source_file)
{
}

Script::~Script()
{
    Teardown();
}

void Script::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);
}

bool Script::Compile(APIInstance &api_instance)
{
    if (!m_source_file.IsValid()) {
        return false;
    }

    // bind all set vars if an api instance has been set
    ScriptBindings::DeclareAll(api_instance);
    api_instance.BindAll(&m_vm, &m_compilation_unit);
    ScriptBindings::RegisterBindings(api_instance);

    SourceStream source_stream(&m_source_file);

    TokenStream token_stream(TokenStreamInfo {
        m_source_file.GetFilePath()
    });

    Lexer lex(source_stream, &token_stream, &m_compilation_unit);
    lex.Analyze();

    AstIterator ast_iterator;

    Builtins builtins;
    builtins.Visit(&m_compilation_unit);

    Parser parser(&ast_iterator, &token_stream, &m_compilation_unit);
    parser.Parse();

    SemanticAnalyzer semantic_analyzer(&ast_iterator, &m_compilation_unit);
    semantic_analyzer.Analyze();

    m_compilation_unit.GetErrorList().SortErrors();
    m_errors = m_compilation_unit.GetErrorList();

    if (!m_compilation_unit.GetErrorList().HasFatalErrors()) {
        // only optimize if there were no errors
        // before this point
        ast_iterator.ResetPosition();
        Optimizer optimizer(&ast_iterator, &m_compilation_unit);
        optimizer.Optimize();

        // compile into bytecode instructions
        ast_iterator.ResetPosition();


        Compiler compiler(&ast_iterator, &m_compilation_unit);

        if (auto builtins_result = builtins.Build(&m_compilation_unit)) {            
            m_bytecode_chunk.Append(std::move(builtins_result));
        } else {
            return false;
        }

        if (auto compile_result = compiler.Compile()) {
            // HYP_BREAKPOINT;
            m_bytecode_chunk.Append(std::move(compile_result));
        } else {
            return false;
        }

        return true;
    }

    return false;
}

InstructionStream Script::Decompile(utf::utf8_ostream *os) const
{
    AssertThrow(IsCompiled() && IsBaked());

    BytecodeStream bytecode_stream(m_baked_bytes.data(), m_baked_bytes.size());

    return DecompilationUnit().Decompile(bytecode_stream, os);
}

void Script::Bake()
{
    AssertThrow(IsCompiled());

    BuildParams build_params { };

    Bake(build_params);
}

void Script::Bake(BuildParams &build_params)
{
    AssertThrow(IsCompiled());

    CodeGenerator code_generator(build_params);
    code_generator.Visit(&m_bytecode_chunk);

    m_baked_bytes = code_generator.GetInternalByteStream().Bake();
    m_bs = BytecodeStream(m_baked_bytes.data(), m_baked_bytes.size());
}

void Script::Run()
{
    AssertThrow(IsCompiled() && IsBaked());

    m_vm.Execute(&m_bs);
}

void Script::CallFunction(const char *name)
{
    CallFunctionArgV(name, nullptr, 0);
}

void Script::CallFunctionArgV(const char *name, Value *args, ArgCount num_args)
{
    CallFunctionArgV(hash_fnv_1(name), args, num_args);    
}

void Script::CallFunction(FunctionHandle handle)
{
    CallFunctionArgV(handle, nullptr, 0);
}

void Script::CallFunction(HashFNV1 hash)
{
    CallFunctionArgV(hash, nullptr, 0);
}

void Script::CallFunctionArgV(HashFNV1 hash, Value *args, ArgCount num_args)
{
    Value handle;
    AssertThrow(GetExportedSymbols().Find(hash, &handle));
    CallFunctionArgV(handle, args, num_args);
}

void Script::CallFunctionArgV(FunctionHandle handle, Value *args, ArgCount num_args)
{
    AssertThrow(IsCompiled() && IsBaked());

    auto *main_thread = m_vm.GetState().GetMainThread();

    if (num_args != 0) {
        AssertThrow(args != nullptr);

        for (ArgCount i = 0; i < num_args; i++) {
            main_thread->m_stack.Push(args[i]);
        }
    }

    m_vm.InvokeNow(
        &m_bs,
        handle,
        num_args
    );

    if (num_args != 0) {
        main_thread->m_stack.Pop(num_args);
    }
}

} // namespace hyperion::v2
