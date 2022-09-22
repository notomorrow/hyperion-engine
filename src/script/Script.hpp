#ifndef SCRIPT_HPP
#define SCRIPT_HPP

#include <script/ScriptApi.hpp>
#include <script/SourceFile.hpp>
#include <script/compiler/ErrorList.hpp>
#include <script/compiler/CompilationUnit.hpp>
#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/InstructionStream.hpp>
#include <script/vm/BytecodeStream.hpp>
#include <script/vm/VM.hpp>

#include <rendering/Base.hpp>

#include <Constants.hpp>
#include <Types.hpp>

#include <util/UTF8.hpp>

#include <Types.hpp>

#include <memory>
#include <array>

namespace hyperion {

using namespace compiler;
using namespace vm;

class APIInstance;

namespace v2 {

class Engine;

class Script : public EngineComponentBase<STUB_CLASS(Script)>
{
public:
    using Bytes = std::vector<UByte>;

    using ArgCount = UInt16;
    using FunctionHandle = Value;

    Script(const SourceFile &source_file);
    ~Script();

    void Init(Engine *engine);

    const ErrorList &GetErrors() const { return m_errors; }

    ExportedSymbolTable &GetExportedSymbols() { return m_vm.GetState().GetExportedSymbols(); }
    const ExportedSymbolTable &GetExportedSymbols() const { return m_vm.GetState().GetExportedSymbols(); }

    VM &GetVM() { return m_vm; }
    const VM &GetVM() const { return m_vm; }

    bool IsBaked() const { return !m_baked_bytes.empty(); }
    bool IsCompiled() const { return !m_bytecode_chunk.buildables.empty(); }

    bool Compile(APIInstance &api_instance);

    InstructionStream Decompile(utf::utf8_ostream *os = nullptr) const;

    void Bake();
    void Bake(BuildParams &build_params);

    void Run();

    template <class T>
    constexpr Value CreateArgument(T &&item)
    {
        using ArgType = std::remove_cv_t<std::decay_t<T>>;

        Value first_value;

        if constexpr (std::is_same_v<int32_t, ArgType>) {
            first_value.m_type = Value::I32;
            first_value.m_value.i32 = item;
        } else if constexpr (std::is_same_v<int64_t, ArgType>) {
            first_value.m_type = Value::I64;
            first_value.m_value.i64 = item;
        } else if constexpr (std::is_same_v<uint32_t, ArgType>) {
            first_value.m_type = Value::U32;
            first_value.m_value.u32 = item;
        } else if constexpr (std::is_same_v<uint64_t, ArgType>) {
            first_value.m_type = Value::U64;
            first_value.m_value.u64 = item;
        } else if constexpr (std::is_same_v<float, ArgType>) {
            first_value.m_type = Value::F32;
            first_value.m_value.f = item;
        } else if constexpr (std::is_same_v<double, ArgType>) {
            first_value.m_type = Value::F64;
            first_value.m_value.d = item;
        } else if constexpr (std::is_pointer_v<ArgType>) {
            // set to userdata
            first_value.m_type = Value::USER_DATA;
            first_value.m_value.user_data = static_cast<void *>(item);
        } else {
            static_assert(resolution_failure<ArgType>, "Cannot pass value type to script function statically!");
        }

        return first_value;
    }

    template <class ...Args>
    constexpr auto CreateArguments(Args &&... args) -> std::array<Value, sizeof...(Args)>
    {
        return std::array<Value, sizeof...(Args)>{
            CreateArgument(args)...
        };
    }

    void CallFunction(FunctionHandle handle);
    void CallFunction(const char *name);
    void CallFunction(HashFNV1 hash);
    void CallFunctionArgV(const char *name, Value *args, ArgCount num_args);
    void CallFunctionArgV(HashFNV1 hash, Value *args, ArgCount num_args);
    void CallFunctionArgV(FunctionHandle handle, Value *args, ArgCount num_args);

    bool GetExportedValue(const char *name, Value *value)
    {
        return GetExportedSymbols().Find(hash_fnv_1(name), value);
    }

    bool GetFunctionHandle(const char *name, FunctionHandle *handle)
    {
        return GetExportedValue(name, handle);
    }

    template <class ...Args>
    void CallFunction(const char *name, Args &&... args)
    {
        auto arguments = CreateArguments(std::forward<Args>(args)...);

        CallFunctionArgV(name, arguments.data(), arguments.size());
    }

    template <class ...Args>
    void CallFunction(FunctionHandle handle, Args &&... args)
    {
        auto arguments = CreateArguments(std::forward<Args>(args)...);

        CallFunctionArgV(handle, arguments.data(), arguments.size());
    }

    template <class ...Args>
    void CallFunction(HashFNV1 hash, Args &&... args)
    {
        auto arguments = CreateArguments(std::forward<Args>(args)...);

        CallFunctionArgV(hash, arguments.data(), arguments.size());
    }

private:
    SourceFile  m_source_file;
    CompilationUnit m_compilation_unit;
    ErrorList m_errors;

    BytecodeChunk m_bytecode_chunk;

    Bytes m_baked_bytes;

    VM m_vm;
    BytecodeStream m_bs;
};

} // namespace v2

} // namespace hyperion

#endif
