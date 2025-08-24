#pragma once

#include <script/ScriptApi.hpp>
#include <script/SourceFile.hpp>
#include <script/compiler/ErrorList.hpp>
#include <script/compiler/CompilationUnit.hpp>
#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/InstructionStream.hpp>
#include <script/vm/BytecodeStream.hpp>

#include <core/containers/FixedArray.hpp>
#include <core/Util.hpp>

#include <core/Constants.hpp>
#include <core/Types.hpp>
#include <core/Defines.hpp>

#include <util/UTF8.hpp>

namespace hyperion {

using namespace compiler;
using namespace vm;

namespace scriptapi2 {
class Context;
} // namespace scriptapi2

class ScriptHandle;

class ValueHandle
{
    friend class HypScript;

    Value _inner = Value(Value::NONE, Value::ValueData { .userData = nullptr });

public:
    bool IsNull() const
    {
        return _inner.m_type == Value::NONE;
    }
};

class HypScript
{

public:
    using ArgCount = uint16;

    static HypScript& GetInstance();

    HypScript();
    HypScript(const HypScript& other) = delete;
    HypScript& operator=(const HypScript& other) = delete;
    ~HypScript();

    APIInstance& GetAPIInstance()
    {
        return m_apiInstance;
    }

    const APIInstance& GetAPIInstance() const
    {
        return m_apiInstance;
    }

    VM* GetVM() const
    {
        return m_vm;
    }

    void Initialize();

    void DestroyScript(ScriptHandle* scriptHandle);

    ScriptHandle* Compile(
        SourceFile& sourceFile,
        ErrorList& outErrorList);

    InstructionStream Decompile(
        ScriptHandle* scriptHandle,
        std::ostream* os = nullptr) const;

    void Run(ScriptHandle* scriptHandle);

    template <class T>
    constexpr Value CreateArgument(T&& item)
    {
        using ArgType = std::remove_cv_t<std::decay_t<T>>;

        Value firstValue;

        if constexpr (std::is_same_v<Value, ArgType>)
        {
            firstValue = item;
        }
        else if constexpr (std::is_base_of_v<ValueHandle, ArgType>)
        {
            firstValue = item._inner;
        }
        else if constexpr (std::is_same_v<int8_t, ArgType>)
        {
            firstValue.m_type = Value::I8;
            firstValue.m_value.i8 = item;
        }
        else if constexpr (std::is_same_v<int16_t, ArgType>)
        {
            firstValue.m_type = Value::I16;
            firstValue.m_value.i16 = item;
        }
        else if constexpr (std::is_same_v<int32_t, ArgType>)
        {
            firstValue.m_type = Value::I32;
            firstValue.m_value.i32 = item;
        }
        else if constexpr (std::is_same_v<int64_t, ArgType>)
        {
            firstValue.m_type = Value::I64;
            firstValue.m_value.i64 = item;
        }
        else if constexpr (std::is_same_v<uint8_t, ArgType>)
        {
            firstValue.m_type = Value::U8;
            firstValue.m_value.u8 = item;
        }
        else if constexpr (std::is_same_v<uint16_t, ArgType>)
        {
            firstValue.m_type = Value::U16;
            firstValue.m_value.u16 = item;
        }
        else if constexpr (std::is_same_v<uint32_t, ArgType>)
        {
            firstValue.m_type = Value::U32;
            firstValue.m_value.u32 = item;
        }
        else if constexpr (std::is_same_v<uint64_t, ArgType>)
        {
            firstValue.m_type = Value::U64;
            firstValue.m_value.u64 = item;
        }
        else if constexpr (std::is_same_v<float, ArgType>)
        {
            firstValue.m_type = Value::F32;
            firstValue.m_value.f = item;
        }
        else if constexpr (std::is_same_v<double, ArgType>)
        {
            firstValue.m_type = Value::F64;
            firstValue.m_value.d = item;
        }
        else if constexpr (std::is_same_v<bool, ArgType>)
        {
            firstValue.m_type = Value::BOOLEAN;
            firstValue.m_value.b = item;
        }
        else if constexpr (std::is_pointer_v<ArgType>)
        {
            // set to userdata
            firstValue.m_type = Value::USER_DATA;
            firstValue.m_value.userData = static_cast<void*>(item);
        }
        else
        {
            static_assert(resolutionFailure<ArgType>, "Cannot pass value type to script function!");
        }

        return firstValue;
    }

    template <class... Args>
    auto CreateArguments(Args&&... args) -> FixedArray<Value, sizeof...(Args)>
    {
        return FixedArray<Value, sizeof...(Args)> {
            CreateArgument(args)...
        };
    }

    void CallFunctionArgV(ScriptHandle* scriptHandle, const Value& function, Value* args, ArgCount numArgs);

    bool GetFunctionHandle(const char* name, Value& outFunction);
    bool GetObjectHandle(const char* name, Value& outObject);

    bool GetExportedValue(const char* name, Value* value);

    ExportedSymbolTable& GetExportedSymbols() const;

    bool GetMember(const Value& objectValue, const char* memberName, Value& outValue);
    bool SetMember(const Value& objectValue, const char* memberName, const Value& value);

    template <class... Args>
    void CallFunction(ScriptHandle* scriptHandle, const Value& function, Args&&... args)
    {
        auto arguments = CreateArguments(std::forward<Args>(args)...);

        CallFunctionArgV(scriptHandle, function, arguments.Data(), arguments.Size());
    }

#if 0
    template <class RegisteredType, class T>
    ValueHandle CreateInternedObject(const T& value)
    {
        const auto classNameIt = m_apiInstance.classBindings.classNames.Find<RegisteredType>();
        Assert(classNameIt != m_apiInstance.classBindings.classNames.End(), "Class %s not registered!", TypeName<RegisteredType>().Data());

        const auto prototypeIt = m_apiInstance.classBindings.classPrototypes.Find(classNameIt->second);
        Assert(prototypeIt != m_apiInstance.classBindings.classPrototypes.End(), "Class %s not registered!", TypeName<RegisteredType>().Data());

        vm::Value internValue;
        {
            vm::HeapValue* ptrResult = m_vm.GetState().HeapAlloc(m_vm.GetState().GetMainThread());
            Assert(ptrResult != nullptr);

            ptrResult->Assign(value);
            ptrResult->Mark();
            internValue = vm::Value(vm::Value::HEAP_POINTER, { .ptr = ptrResult });
        }

        vm::VMObject boxedValue(prototypeIt->second);
        HYP_SCRIPT_SET_MEMBER(boxedValue, "__intern", internValue);

        vm::Value finalValue;
        {
            vm::HeapValue* ptrResult = m_vm.GetState().HeapAlloc(m_vm.GetState().GetMainThread());
            Assert(ptrResult != nullptr);

            ptrResult->Assign(boxedValue);
            ptrResult->Mark();
            finalValue = vm::Value(vm::Value::HEAP_POINTER, { .ptr = ptrResult });
        }

        return { finalValue };
    }
#endif

private:
    scriptapi2::Context m_context;
    APIInstance m_apiInstance;
    VM* m_vm;
};

} // namespace hyperion
