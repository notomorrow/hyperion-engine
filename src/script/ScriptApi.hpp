#pragma once

#include <script/vm/VMObject.hpp>
#include <script/vm/VMArray.hpp>
#include <script/vm/VMMap.hpp>
#include <script/vm/VMString.hpp>
#include <script/vm/Value.hpp>

#include <script/compiler/Configuration.hpp>
#include <script/compiler/CompilationUnit.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/type-system/SymbolType.hpp>
#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/ScriptApi2.hpp>

#include <core/utilities/Variant.hpp>
#include <core/containers/TypeMap.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/LinkedList.hpp>
#include <core/containers/String.hpp>
#include <core/utilities/Optional.hpp>
#include <core/memory/Any.hpp>
#include <core/Util.hpp>

#include <core/Types.hpp>

#include <core/Defines.hpp>

#ifndef __cplusplus
#error Script requires a C++ compiler
#endif

#pragma region Script helper macros

#define HYP_SCRIPT_PARAMS hyperion::sdk::Params

#define HYP_SCRIPT_EXPORT_FUNCTION(name) HYP_EXPORT void name(hyperion::sdk::Params params)

#define HYP_SCRIPT_FUNCTION(name) void name(hyperion::sdk::Params params)

#define HYP_SCRIPT_CHECK_ARGS(cmp, amt)                                                      \
    do                                                                                       \
    {                                                                                        \
        if (!(params.nargs cmp amt))                                                         \
        {                                                                                    \
            params.handler->state->ThrowException(params.handler->thread,                    \
                hyperion::vm::Exception::InvalidArgsException(#cmp " " #amt, params.nargs)); \
            return;                                                                          \
        }                                                                                    \
    }                                                                                        \
    while (false)

#define HYP_SCRIPT_THROW(exception)            \
    do                                         \
    {                                          \
        params.handler->state->ThrowException( \
            params.handler->thread,            \
            exception);                        \
        return;                                \
    }                                          \
    while (0)

#define HYP_SCRIPT_RETURN(value)                           \
    do                                                     \
    {                                                      \
        params.handler->thread->GetRegisters()[0] = value; \
        return;                                            \
    }                                                      \
    while (false)

#define HYP_SCRIPT_RETURN_PTR(value)           \
    do                                         \
    {                                          \
        Assert(value != nullptr);              \
        value->Mark();                         \
        vm::Value _res;                        \
        _res.m_type = vm::Value::HEAP_POINTER; \
        _res.m_value.internal.ptr = value;              \
        HYP_SCRIPT_RETURN(_res);               \
    }                                          \
    while (false)

#define HYP_SCRIPT_RETURN_NULL()               \
    do                                         \
    {                                          \
        vm::Value _res;                        \
        _res.m_type = vm::Value::HEAP_POINTER; \
        _res.m_value.internal.ptr = nullptr;            \
        HYP_SCRIPT_RETURN(_res);               \
    }                                          \
    while (false)

#define HYP_SCRIPT_RETURN_VOID(value)  \
    do                                 \
    {                                  \
        (void)(value);                 \
        vm::Value _res;                \
        _res.m_type = vm::Value::NONE; \
        HYP_SCRIPT_RETURN(_res);       \
    }                                  \
    while (false)

#define HYP_SCRIPT_RETURN_INT32(value) \
    do                                 \
    {                                  \
        vm::Value _res;                \
        _res.m_type = vm::Value::I32;  \
        _res.m_value.i32 = value;      \
        HYP_SCRIPT_RETURN(_res);       \
    }                                  \
    while (false)

#define HYP_SCRIPT_RETURN_INT64(value) \
    do                                 \
    {                                  \
        vm::Value _res;                \
        _res.m_type = vm::Value::I64;  \
        _res.m_value.i64 = value;      \
        HYP_SCRIPT_RETURN(_res);       \
    }                                  \
    while (false)

#define HYP_SCRIPT_RETURN_UINT32(value) \
    do                                  \
    {                                   \
        vm::Value _res;                 \
        _res.m_type = vm::Value::U32;   \
        _res.m_value.u32 = value;       \
        HYP_SCRIPT_RETURN(_res);        \
    }                                   \
    while (false)

#define HYP_SCRIPT_RETURN_UINT64(value) \
    do                                  \
    {                                   \
        vm::Value _res;                 \
        _res.m_type = vm::Value::U64;   \
        _res.m_value.u64 = value;       \
        HYP_SCRIPT_RETURN(_res);        \
    }                                   \
    while (false)

#define HYP_SCRIPT_RETURN_FLOAT32(value) \
    do                                   \
    {                                    \
        vm::Value _res;                  \
        _res.m_type = vm::Value::F32;    \
        _res.m_value.f = value;          \
        HYP_SCRIPT_RETURN(_res);         \
    }                                    \
    while (false)

#define HYP_SCRIPT_RETURN_FLOAT64(value) \
    do                                   \
    {                                    \
        vm::Value _res;                  \
        _res.m_type = vm::Value::F64;    \
        _res.m_value.d = value;          \
        HYP_SCRIPT_RETURN(_res);         \
    }                                    \
    while (false)

#define HYP_SCRIPT_RETURN_BOOLEAN(value)  \
    do                                    \
    {                                     \
        vm::Value _res;                   \
        _res.m_type = vm::Value::BOOLEAN; \
        _res.m_value.b = value;           \
        HYP_SCRIPT_RETURN(_res);          \
    }                                     \
    while (false)

#define HYP_SCRIPT_GET_ARG_INT(index, name)                                                                                                                  \
    int64 name;                                                                                                                                              \
    do                                                                                                                                                       \
    {                                                                                                                                                        \
        vm::Number num;                                                                                                                                      \
        if (!params.args[index]->GetSignedOrUnsigned(&num))                                                                                                  \
        {                                                                                                                                                    \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be of type int or uint")); \
            return (decltype(name))0;                                                                                                                        \
        }                                                                                                                                                    \
        name = (num.flags & vm::Number::FLAG_UNSIGNED) ? static_cast<int64>(num.u) : num.i;                                                                  \
    }                                                                                                                                                        \
    while (false)

#define HYP_SCRIPT_GET_ARG_UINT(index, name)                                                                                                                 \
    uint64 name;                                                                                                                                             \
    do                                                                                                                                                       \
    {                                                                                                                                                        \
        vm::Number num;                                                                                                                                      \
        if (!params.args[index]->GetSignedOrUnsigned(&num))                                                                                                  \
        {                                                                                                                                                    \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be of type int or uint")); \
            return (decltype(name))0;                                                                                                                        \
        }                                                                                                                                                    \
        name = (num.flags & vm::Number::FLAG_SIGNED) ? static_cast<uint64>(num.i) : num.u;                                                                   \
    }                                                                                                                                                        \
    while (false)

#define HYP_SCRIPT_GET_ARG_BOOLEAN(index, name)                                                                                                       \
    bool name;                                                                                                                                        \
    do                                                                                                                                                \
    {                                                                                                                                                 \
        if (!params.args[index]->GetBoolean(&name))                                                                                                   \
        {                                                                                                                                             \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be of type bool")); \
            return (decltype(name))0;                                                                                                                 \
        }                                                                                                                                             \
    }                                                                                                                                                 \
    while (false)

#define HYP_SCRIPT_GET_ARG_FLOAT(index, name)                                                                                                          \
    double name;                                                                                                                                       \
    do                                                                                                                                                 \
    {                                                                                                                                                  \
        if (!params.args[index]->GetFloatingPointCoerce(&name))                                                                                        \
        {                                                                                                                                              \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be of type float")); \
            return (decltype(name))0;                                                                                                                  \
        }                                                                                                                                              \
    }                                                                                                                                                  \
    while (false)

#define HYP_SCRIPT_GET_ARG_PTR(index, type, name)                                                                                                       \
    type* name;                                                                                                                                         \
    do                                                                                                                                                  \
    {                                                                                                                                                   \
        if (!params.args[index]->GetPointer<type>(&name))                                                                                               \
        {                                                                                                                                               \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be of type " #type)); \
        }                                                                                                                                               \
    }                                                                                                                                                   \
    while (false)

#define HYP_SCRIPT_GET_ARG_PTR_RAW(index, type, name)                                                                                                   \
    type* name;                                                                                                                                         \
    do                                                                                                                                                  \
    {                                                                                                                                                   \
        if (!params.args[index]->GetUserData<type>(&name))                                                                                              \
        {                                                                                                                                               \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be of type " #type)); \
            return (decltype(name))nullptr;                                                                                                             \
        }                                                                                                                                               \
    }                                                                                                                                                   \
    while (false)

#define HYP_SCRIPT_GET_ARG_INT_1(index, name)                                                                                                    \
    int64 name;                                                                                                                                  \
    do                                                                                                                                           \
    {                                                                                                                                            \
        auto _value = ConvertScriptObject<int64>(params.apiInstance, *params.args[index]);                                                       \
        if (!_value.HasValue())                                                                                                                  \
        {                                                                                                                                        \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be numeric")); \
            return (decltype(name))0;                                                                                                            \
        }                                                                                                                                        \
        name = _value.Get();                                                                                                                     \
    }                                                                                                                                            \
    while (false)

#define HYP_SCRIPT_GET_ARG_UINT_1(index, name)                                                                                                  \
    uint64 name;                                                                                                                                \
    do                                                                                                                                          \
    {                                                                                                                                           \
        auto _value = ConvertScriptObject<uint64>(params.apiInstance, *params.args[index]);                                                     \
        if (!_value.HasValue())                                                                                                                 \
        {                                                                                                                                       \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be number")); \
            return (decltype(name))0;                                                                                                           \
        }                                                                                                                                       \
        name = _value.Get();                                                                                                                    \
    }                                                                                                                                           \
    while (false)

#define HYP_SCRIPT_GET_ARG_BOOLEAN_1(index, name)                                                                                                     \
    bool name;                                                                                                                                        \
    do                                                                                                                                                \
    {                                                                                                                                                 \
        auto _value = ConvertScriptObject<bool>(params.apiInstance, *params.args[index]);                                                             \
        if (!_value.HasValue())                                                                                                                       \
        {                                                                                                                                             \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be of type bool")); \
            return (decltype(name))0;                                                                                                                 \
        }                                                                                                                                             \
        name = _value.Get();                                                                                                                          \
    }                                                                                                                                                 \
    while (false)

#define HYP_SCRIPT_GET_ARG_FLOAT_1(index, name)                                                                                                  \
    double name;                                                                                                                                 \
    do                                                                                                                                           \
    {                                                                                                                                            \
        auto _value = ConvertScriptObject<double>(params.apiInstance, *params.args[index]);                                                      \
        if (!_value.HasValue())                                                                                                                  \
        {                                                                                                                                        \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be numeric")); \
            return (decltype(name))0;                                                                                                            \
        }                                                                                                                                        \
        name = _value.Get();                                                                                                                     \
    }                                                                                                                                            \
    while (false)

#define HYP_SCRIPT_GET_ARG_PTR_1(index, type, name)                                                                                                               \
    type* name;                                                                                                                                                   \
    do                                                                                                                                                            \
    {                                                                                                                                                             \
        auto _value = ConvertScriptObject<type*>(params.apiInstance, *params.args[index]);                                                                        \
        if (!_value.HasValue())                                                                                                                                   \
        {                                                                                                                                                         \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be a pointer of type " #type)); \
        }                                                                                                                                                         \
        name = _value.Get();                                                                                                                                      \
    }                                                                                                                                                             \
    while (false)

#define HYP_SCRIPT_GET_ARG_PTR_RAW_1(index, type, name)                                                                                                    \
    type* name;                                                                                                                                            \
    do                                                                                                                                                     \
    {                                                                                                                                                      \
        auto _value = ConvertScriptObject<void*>(params.apiInstance, *params.args[index]);                                                                 \
        if (!_value.HasValue())                                                                                                                            \
        {                                                                                                                                                  \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected argument at index " #index " to be userdata (void *)")); \
            return (decltype(name))0;                                                                                                                      \
        }                                                                                                                                                  \
        name = static_cast<type*>(_value.Get());                                                                                                           \
    }                                                                                                                                                      \
    while (false)

#define HYP_SCRIPT_GET_MEMBER_INT(object, name, type, declName)                                                                                 \
    type declName;                                                                                                                              \
    do                                                                                                                                          \
    {                                                                                                                                           \
        vm::Number num;                                                                                                                         \
        vm::Member* _member = nullptr;                                                                                                          \
        if (!(_member = object->LookupMemberFromHash(hashFnv1(name))) || !_member->value.GetSignedOrUnsigned(&num))                             \
        {                                                                                                                                       \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected member " name " to be of type int or uint")); \
            return;                                                                                                                             \
        }                                                                                                                                       \
        declName = (num.flags & vm::Number::FLAG_UNSIGNED) ? static_cast<int64>(num.u) : num.i;                                                 \
    }                                                                                                                                           \
    while (false)

#define HYP_SCRIPT_GET_MEMBER_UINT(object, name, type, declName)                                                                         \
    type declName;                                                                                                                       \
    do                                                                                                                                   \
    {                                                                                                                                    \
        uint64 num;                                                                                                                      \
        vm::Member* _member = nullptr;                                                                                                   \
        if (!(_member = object->LookupMemberFromHash(hashFnv1(name))) || !_member->value.GetUnsigned(&num))                              \
        {                                                                                                                                \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected member " name " to be of type uint")); \
            return;                                                                                                                      \
        }                                                                                                                                \
        declName = static_cast<type>(num);                                                                                               \
    }                                                                                                                                    \
    while (false)

#define HYP_SCRIPT_GET_MEMBER_FLOAT(object, name, type, declName)                                                                         \
    type declName;                                                                                                                        \
    do                                                                                                                                    \
    {                                                                                                                                     \
        double num;                                                                                                                       \
        vm::Member* _member = nullptr;                                                                                                    \
        if (!(_member = object->LookupMemberFromHash(hashFnv1(name))) || !_member->value.GetFloatingPointCoerce(&num))                    \
        {                                                                                                                                 \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected member " name " to be of type float")); \
            return;                                                                                                                       \
        }                                                                                                                                 \
        declName = static_cast<type>(num);                                                                                                \
    }                                                                                                                                     \
    while (false)

#define HYP_SCRIPT_GET_MEMBER_PTR(object, name, type, declName)                                                                            \
    type* declName;                                                                                                                        \
    do                                                                                                                                     \
    {                                                                                                                                      \
        vm::Member* _member = nullptr;                                                                                                     \
        if (!(_member = object->LookupMemberFromHash(hashFnv1(name))) || !_member->value.GetPointer<type>(&declName))                      \
        {                                                                                                                                  \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected member " name " to be of type " #type)); \
        }                                                                                                                                  \
    }                                                                                                                                      \
    while (false)

#define HYP_SCRIPT_GET_MEMBER_PTR_RAW(object, name, type, declName)                                                                        \
    type* declName;                                                                                                                        \
    do                                                                                                                                     \
    {                                                                                                                                      \
        vm::Member* _member = nullptr;                                                                                                     \
        if (!(_member = object->LookupMemberFromHash(hashFnv1(name))) || !_member->value.GetUserData<type>(&declName))                     \
        {                                                                                                                                  \
            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Expected member " name " to be of type " #type)); \
            return;                                                                                                                        \
        }                                                                                                                                  \
    }                                                                                                                                      \
    while (false)

#define HYP_SCRIPT_CREATE_PTR(assignment, outName)                                           \
    vm::Value outName;                                                                       \
    do                                                                                       \
    {                                                                                        \
        vm::HeapValue* ptrResult = params.handler->state->HeapAlloc(params.handler->thread); \
        Assert(ptrResult != nullptr);                                                        \
                                                                                             \
        ptrResult->Assign((assignment));                                                     \
        ptrResult->Mark();                                                                   \
                                                                                             \
        outName = vm::Value(vm::Value::HEAP_POINTER, { .internal = { .ptr = ptrResult } });                  \
    }                                                                                        \
    while (false)

#define HYP_SCRIPT_SET_MEMBER(object, nameStr, assignment)                                                  \
    do                                                                                                      \
    {                                                                                                       \
        auto* member = object.LookupMemberFromHash(hashFnv1(nameStr));                                      \
        Assert(member != nullptr, "Member " nameStr " not set on object, unmatching prototype definition"); \
        member->value = assignment;                                                                         \
    }                                                                                                       \
    while (false)

#pragma endregion

namespace hyperion {

namespace vm {
// forward declarations
class VM;
struct VMState;
// struct Value;
struct Script_ExecutionThread;
class InstructionHandler;
class VMString;

} // namespace vm

using namespace compiler;

class APIInstance;

template <class T>
constexpr bool isVmObjectType = std::is_same_v<vm::VMString, T> || std::is_same_v<vm::VMObject, T>;

#pragma region Script API Instance

class APIInstance
{
public:
    struct ClassBindings
    {
        TypeMap<String> classNames;
        HashMap<String, vm::HeapValue*> classPrototypes;
    } classBindings;

    APIInstance();
    APIInstance(const APIInstance& other) = delete;
    ~APIInstance() = default;

    vm::VM* GetVM() const
    {
        return m_vm;
    }

    void SetVM(vm::VM* vm)
    {
        m_vm = vm;
    }

private:
    vm::VM* m_vm;
};

#pragma endregion

#pragma region Conversion Helpers

template <class T>
struct CxxToScriptValueImpl;

template <class T>
static inline vm::Value CxxToScriptValueInternal(APIInstance& apiInstance, T&& value)
{
    CxxToScriptValueImpl<NormalizedType<T>> impl;

    return impl(apiInstance, std::forward<T>(value));
}

template <class T>
struct CxxToScriptValueImpl
{
    static_assert(std::is_class_v<T> || std::is_pointer_v<T>, "Must be a class type or pointer type");
    static_assert(!isVmObjectType<T>, "Should not receive a VM object type as it is handled by other specializations");

    // use a template param to allow const references and similar but still having T be decayed
    template <class Ty>
    vm::Value operator()(APIInstance& apiInstance, Ty&& value) const
    {
        static_assert(std::is_convertible_v<Ty, T>, "must be convertible; unrelated types T and Ty");

        if constexpr (std::is_pointer_v<T>)
        {
            vm::Value finalValue;
            finalValue.m_type = vm::Value::USER_DATA;
            finalValue.m_value.internal.userData = value;

            return finalValue;
        }
        else
        {
            const auto classNameIt = apiInstance.classBindings.classNames.Find<T>();
            Assert(classNameIt != apiInstance.classBindings.classNames.End(), "Class not registered!");

            const auto prototypeIt = apiInstance.classBindings.classPrototypes.Find(classNameIt->second);
            Assert(prototypeIt != apiInstance.classBindings.classPrototypes.End(), "Class not registered!");

            vm::Value internValue;
            {
                vm::HeapValue* ptrResult = apiInstance.GetVM()->GetState().HeapAlloc(apiInstance.GetVM()->GetState().GetMainThread());
                Assert(ptrResult != nullptr);

                ptrResult->Assign(std::forward<Ty>(value));
                ptrResult->Mark();
                internValue = vm::Value(vm::Value::HEAP_POINTER, { .internal = { .ptr = ptrResult } });
            }

            vm::VMObject boxedValue(prototypeIt->second);
            HYP_SCRIPT_SET_MEMBER(boxedValue, "__intern", internValue);

            vm::Value finalValue;
            {
                vm::HeapValue* ptrResult = apiInstance.GetVM()->GetState().HeapAlloc(apiInstance.GetVM()->GetState().GetMainThread());
                Assert(ptrResult != nullptr);

                ptrResult->Assign(boxedValue);
                ptrResult->Mark();
                finalValue = vm::Value(vm::Value::HEAP_POINTER, { .internal = { .ptr = ptrResult } });
            }

            return finalValue;
        }
    }
};

/*! \brief Identity conversion for VM values. No conversion is performed. */
template <>
struct CxxToScriptValueImpl<vm::Value>
{
    vm::Value operator()(APIInstance&, vm::Value value) const
    {
        return value;
    }
};

template <>
struct CxxToScriptValueImpl<int8>
{
    vm::Value operator()(APIInstance&, int8 value) const
    {
        return vm::Value(vm::Value::I8, { .i8 = value });
    }
};

template <>
struct CxxToScriptValueImpl<int16>
{
    vm::Value operator()(APIInstance&, int16 value) const
    {
        return vm::Value(vm::Value::I16, { .i16 = value });
    }
};

template <>
struct CxxToScriptValueImpl<int32>
{
    vm::Value operator()(APIInstance&, int32 value) const
    {
        return vm::Value(vm::Value::I32, { .i32 = value });
    }
};

template <>
struct CxxToScriptValueImpl<int64>
{
    vm::Value operator()(APIInstance&, int64 value) const
    {
        return vm::Value(vm::Value::I64, { .i64 = value });
    }
};

template <>
struct CxxToScriptValueImpl<uint8>
{
    vm::Value operator()(APIInstance&, uint8 value) const
    {
        return vm::Value(vm::Value::U8, { .u8 = value });
    }
};

template <>
struct CxxToScriptValueImpl<uint16>
{
    vm::Value operator()(APIInstance&, uint16 value) const
    {
        return vm::Value(vm::Value::U16, { .u16 = value });
    }
};

template <>
struct CxxToScriptValueImpl<uint32>
{
    vm::Value operator()(APIInstance&, uint32 value) const
    {
        return vm::Value(vm::Value::U32, { .u32 = value });
    }
};

template <>
struct CxxToScriptValueImpl<uint64>
{
    vm::Value operator()(APIInstance&, uint64 value) const
    {
        return vm::Value(vm::Value::U64, { .u64 = value });
    }
};

template <>
struct CxxToScriptValueImpl<float>
{
    vm::Value operator()(APIInstance&, float value) const
    {
        return vm::Value(vm::Value::F32, { .f = value });
    }
};

template <>
struct CxxToScriptValueImpl<double>
{
    vm::Value operator()(APIInstance&, double value) const
    {
        return vm::Value(vm::Value::F64, { .d = value });
    }
};

template <>
struct CxxToScriptValueImpl<bool>
{
    vm::Value operator()(APIInstance&, bool value) const
    {
        return vm::Value(vm::Value::BOOLEAN, { .b = value });
    }
};

template <>
struct CxxToScriptValueImpl<String>
{
    vm::Value operator()(APIInstance& apiInstance, const String& value) const
    {
        vm::VMString str = value;

        vm::Value finalValue;
        {
            vm::HeapValue* ptrResult = apiInstance.GetVM()->GetState().HeapAlloc(apiInstance.GetVM()->GetState().GetMainThread());
            Assert(ptrResult != nullptr);

            ptrResult->Assign(std::move(str));
            ptrResult->Mark();
            finalValue = vm::Value(vm::Value::HEAP_POINTER, { .internal = { .ptr = ptrResult } });
        }

        return finalValue;
    }
};

// @TODO DynArray -> VMArray

template <class T>
struct ScriptToCxxValueImpl;

template <class T>
static inline auto ConvertScriptObjectInternal(APIInstance& apiInstance, const vm::Value& value)
{
    ScriptToCxxValueImpl<T> impl;

    return impl(apiInstance, value);
}

template <class T, class Ty = NormalizedType<T>>
static inline auto ConvertScriptObject(APIInstance& apiInstance, const vm::Value& value)
    -> std::enable_if_t<!std::is_enum_v<Ty>, decltype(ConvertScriptObjectInternal<Ty>(apiInstance, value))>
{
    return ConvertScriptObjectInternal<Ty>(apiInstance, value);
}

template <class T, class Ty = NormalizedType<T>>
static inline auto ConvertScriptObject(APIInstance& apiInstance, const vm::Value& value)
    -> std::enable_if_t<std::is_enum_v<Ty>, decltype(ConvertScriptObjectInternal<std::underlying_type_t<Ty>>(apiInstance, value))>
{
    return ConvertScriptObjectInternal<std::underlying_type_t<Ty>>(apiInstance, value);
}

template <class T>
struct ScriptToCxxValueImpl
{
    static_assert(!isVmObjectType<NormalizedType<T>>, "Should not receive a VM object type as it is handled by other specializations");

    Optional<T> operator()(APIInstance&, const vm::Value& value) const
    {
        vm::VMObject* object = nullptr;

        if (!value.GetPointer<vm::VMObject>(&object))
        {
            return {};
        }

        vm::Member* member;

        T* tPtr = nullptr;

        if ((member = object->LookupMemberFromHash(hashFnv1("__intern"))) && member->value.GetPointer<T>(&tPtr))
        {
            return *tPtr;
        }

        return {};
    }
};

/*! \brief Identity conversion for VM values. No conversion is performed. */
template <>
struct ScriptToCxxValueImpl<vm::Value>
{
    Optional<vm::Value> operator()(APIInstance&, const vm::Value& value) const
    {
        return { value };
    }
};

/*! \brief Conversion from VM values to int32 */
template <>
struct ScriptToCxxValueImpl<int32>
{
    Optional<int32> operator()(APIInstance&, const vm::Value& value) const
    {
        vm::Number num {};

        if (value.GetSignedOrUnsigned(&num))
        {
            return int32((num.flags & vm::Number::FLAG_UNSIGNED) ? num.u : num.i);
        }

        return {};
    }
};

/*! \brief Conversion from VM values to int64 */
template <>
struct ScriptToCxxValueImpl<int64>
{
    Optional<int64> operator()(APIInstance&, const vm::Value& value) const
    {
        vm::Number num {};

        if (value.GetSignedOrUnsigned(&num))
        {
            return int64((num.flags & vm::Number::FLAG_UNSIGNED) ? num.u : num.i);
        }

        return {};
    }
};

/*! \brief Conversion from VM values to uint32 */
template <>
struct ScriptToCxxValueImpl<uint32>
{
    Optional<uint32> operator()(APIInstance&, const vm::Value& value) const
    {
        vm::Number num {};

        if (value.GetSignedOrUnsigned(&num))
        {
            return uint32((num.flags & vm::Number::FLAG_UNSIGNED) ? num.u : num.i);
        }

        return {};
    }
};

/*! \brief Conversion from VM values to uint64 */
template <>
struct ScriptToCxxValueImpl<uint64>
{
    Optional<uint64> operator()(APIInstance&, const vm::Value& value) const
    {
        vm::Number num {};

        if (value.GetSignedOrUnsigned(&num))
        {
            return uint64((num.flags & vm::Number::FLAG_UNSIGNED) ? num.u : num.i);
        }

        return {};
    }
};

/*! \brief Conversion from VM values to float */
template <>
struct ScriptToCxxValueImpl<float>
{
    Optional<float> operator()(APIInstance&, const vm::Value& value) const
    {
        double dbl;

        if (value.GetFloatingPointCoerce(&dbl))
        {
            return float(dbl);
        }

        return {};
    }
};

/*! \brief Conversion from VM values to double */
template <>
struct ScriptToCxxValueImpl<double>
{
    Optional<double> operator()(APIInstance&, const vm::Value& value) const
    {
        double dbl;

        if (value.GetFloatingPointCoerce(&dbl))
        {
            return dbl;
        }

        return {};
    }
};

/*! \brief Conversion from VM values to bool */
template <>
struct ScriptToCxxValueImpl<bool>
{
    Optional<bool> operator()(APIInstance&, const vm::Value& value) const
    {
        bool b;

        if (value.GetBoolean(&b))
        {
            return b;
        }

        return {};
    }
};

/*! \brief Conversion from VM values to String */
template <>
struct ScriptToCxxValueImpl<String>
{
    Optional<String> operator()(APIInstance&, const vm::Value& value) const
    {
        vm::VMString* str = nullptr;

        if (value.GetPointer<vm::VMString>(&str))
        {
            return String(str->GetData());
        }

        return {};
    }
};

/*! \brief Conversion from VM values to void * */
template <>
struct ScriptToCxxValueImpl<void*>
{
    Optional<void*> operator()(APIInstance&, const vm::Value& value) const
    {
        if (value.m_type == vm::Value::ValueType::HEAP_POINTER)
        {
            return GetRawPointerForHeapValue(value.m_value.internal.ptr);
        }

        if (value.m_type == vm::Value::ValueType::USER_DATA)
        {
            return value.m_value.internal.userData;
        }

        return {};
    }
};

/*! \brief Conversion from VM values to const void * */
template <>
struct ScriptToCxxValueImpl<const void*>
{
    Optional<const void*> operator()(APIInstance& apiInstance, const vm::Value& value) const
    {
        Optional<void*> result = ScriptToCxxValueImpl<void*>()(apiInstance, value);

        if (!result.HasValue())
        {
            return {};
        }

        return {
            const_cast<const void*>(ScriptToCxxValueImpl<void*>()(apiInstance, value).GetOr(nullptr))
        };
    }
};

template <>
struct ScriptToCxxValueImpl<vm::VMString*>
{
    Optional<vm::VMString*> operator()(APIInstance&, const vm::Value& value) const
    {
        vm::VMString* ptr;

        if (value.GetPointer<vm::VMString>(&ptr))
        {
            return ptr;
        }

        return {};
    }
};

template <>
struct ScriptToCxxValueImpl<vm::VMObject*>
{
    Optional<vm::VMObject*> operator()(APIInstance&, const vm::Value& value) const
    {
        vm::VMObject* ptr;

        if (value.GetPointer<vm::VMObject>(&ptr))
        {
            return ptr;
        }

        return {};
    }
};

// template <>
// struct ScriptToCxxValueImpl<vm::VMArray *>
// {
//     Optional<vm::VMArray *> operator()(APIInstance &, const vm::Value &value) const
//     {
//         vm::VMArray *ptr;

//         if (value.GetPointer<vm::VMArray>(&ptr)) {
//             return ptr;
//         }

//         return { };
//     }
// };

// template <>
// struct ScriptToCxxValueImpl<vm::VMMap *>
// {
//     Optional<vm::VMMap *> operator()(APIInstance &, const vm::Value &value) const
//     {
//         vm::VMMap *ptr;

//         if (value.GetPointer<vm::VMMap>(&ptr)) {
//             return ptr;
//         }

//         return { };
//     }
// };

template <>
struct ScriptToCxxValueImpl<vm::VMString>
{
    vm::VMString* operator()(APIInstance&, const vm::Value& value) const
    {
        vm::VMString* ptr;

        if (value.GetPointer<vm::VMString>(&ptr))
        {
            return ptr;
        }

        return nullptr;
    }
};

template <>
struct ScriptToCxxValueImpl<vm::VMObject>
{
    vm::VMObject* operator()(APIInstance&, const vm::Value& value) const
    {
        vm::VMObject* ptr;

        if (value.GetPointer<vm::VMObject>(&ptr))
        {
            return ptr;
        }

        return nullptr;
    }
};

// template <>
// struct ScriptToCxxValueImpl<vm::VMArray>
// {
//     vm::VMArray *operator()(APIInstance &, const vm::Value &value) const
//     {
//         vm::VMArray *ptr;

//         if (value.GetPointer<vm::VMArray>(&ptr)) {
//             return ptr;
//         }

//         return nullptr;
//     }
// };

// template <>
// struct ScriptToCxxValueImpl<vm::VMMap>
// {
//     vm::VMMap *operator()(APIInstance &, const vm::Value &value) const
//     {
//         vm::VMMap *ptr;

//         if (value.GetPointer<vm::VMMap>(&ptr)) {
//             return ptr;
//         }

//         return nullptr;
//     }
// };

template <class T>
struct ScriptToCxxValueImpl<T*> // embedded C++ object (as pointer)
{
    static_assert(!isVmObjectType<NormalizedType<std::remove_pointer_t<T>>>, "Should not receive a VM object type as it is handled by other specializations");

    Optional<T*> operator()(APIInstance&, const vm::Value& value) const
    {
        vm::VMObject* object = nullptr;

        if (!value.GetPointer<vm::VMObject>(&object))
        {
            DebugLog(LogType::Warn, "Unable to convert VM value to C++ object pointer of type %s\n", TypeName<T>().Data());

            return {};
        }

        vm::Member* member;

        T* tPtr = nullptr;

        if ((member = object->LookupMemberFromHash(hashFnv1("__intern"))) && member->value.GetPointer<T>(&tPtr))
        {
            return tPtr;
        }

        DebugLog(LogType::Warn, "Unable to convert VM value to C++ object pointer of type %s: No __intern value or __intern was not set to a %s pointer\n", TypeName<T>().Data(), TypeName<T>().Data());

        return {};
    }
};

#if 0

template <>
struct ScriptToCxxValueImpl<AnyPtr>
{
    Optional<AnyPtr> operator()(APIInstance &apiInstance, const vm::Value &value) const
    {
        AnyPtr anyPtr = value.ToAnyPtr();

        if (anyPtr.Is<void>()) {
            return { };
        }

        return anyPtr;
    }
};

#endif

template <class T, class AllocatorType>
struct ScriptToCxxValueImpl<Array<T, AllocatorType>>
{
    Optional<Array<T, AllocatorType>> operator()(APIInstance& apiInstance, const vm::Value& value) const
    {
        vm::VMArray* ary = nullptr;

        if (value.GetPointer<vm::VMArray>(&ary))
        {
            Array<T, AllocatorType> result;

            result.Resize(ary->GetSize());

            for (SizeType index = 0; index < ary->GetSize(); index++)
            {
                ScriptToCxxValueImpl<T> elementConvertImpl;

                auto elementResult = elementConvertImpl(apiInstance, ary->AtIndex(index));

                if (!elementResult.HasValue())
                {
                    return {};
                }

                result[index] = std::move(elementResult.Get());
            }

            return Optional<Array<T, AllocatorType>>(std::move(result));
        }

        return {};
    }
};

/*template <class T>
static inline auto ConvertScriptObject(APIInstance &apiInstance, const vm::Value &value, T &&out) -> std::enable_if_t<std::is_class_v<NormalizedType<std::remove_pointer_t<T>>> && !(isVmObjectType<NormalizedType<std::remove_pointer_t<T>>>), bool>
{
    ScriptToCxxValueImpl<ScriptClassWrapper<NormalizedType<std::remove_pointer_t<T>>>> impl;

    return impl(apiInstance, value, out);
}*/

#pragma region Get Argument Helpers

template <class T>
struct GetArgumentImpl
{
    // static_assert(resolutionFailure<T>, "Unable to use type as arg");

    auto operator()(int index, sdk::Params& params)
    {
        HYP_SCRIPT_GET_ARG_PTR_1(index, T, argValue);

        return argValue;
    }
};

template <>
struct GetArgumentImpl<int32>
{
    auto operator()(int index, sdk::Params& params)
    {
        HYP_SCRIPT_GET_ARG_INT_1(index, argValue);

        return argValue;
    }
};

template <>
struct GetArgumentImpl<int64>
{
    auto operator()(int index, sdk::Params& params)
    {
        HYP_SCRIPT_GET_ARG_INT_1(index, argValue);

        return argValue;
    }
};

template <>
struct GetArgumentImpl<uint32>
{
    auto operator()(int index, sdk::Params& params)
    {
        HYP_SCRIPT_GET_ARG_UINT_1(index, argValue);

        return argValue;
    }
};

template <>
struct GetArgumentImpl<uint64>
{
    auto operator()(int index, sdk::Params& params)
    {
        HYP_SCRIPT_GET_ARG_UINT_1(index, argValue);

        return argValue;
    }
};

template <>
struct GetArgumentImpl<float>
{
    auto operator()(int index, sdk::Params& params)
    {
        HYP_SCRIPT_GET_ARG_FLOAT_1(index, argValue);

        return argValue;
    }
};

template <>
struct GetArgumentImpl<double>
{
    auto operator()(int index, sdk::Params& params)
    {
        HYP_SCRIPT_GET_ARG_FLOAT_1(index, argValue);

        return argValue;
    }
};

template <>
struct GetArgumentImpl<bool>
{
    auto operator()(int index, sdk::Params& params)
    {
        HYP_SCRIPT_GET_ARG_BOOLEAN_1(index, argValue);

        return argValue;
    }
};

template <>
struct GetArgumentImpl<void*>
{
    auto operator()(int index, sdk::Params& params)
    {
        HYP_SCRIPT_GET_ARG_PTR_RAW_1(index, void, argValue);

        return argValue;
    }
};

template <>
struct GetArgumentImpl<vm::VMString>
{
    auto operator()(int index, sdk::Params& params)
    {
        HYP_SCRIPT_GET_ARG_PTR_1(index, vm::VMString, argValue);

        return argValue;
    }
};

template <>
struct GetArgumentImpl<vm::VMObject>
{
    auto operator()(int index, sdk::Params& params)
    {
        HYP_SCRIPT_GET_ARG_PTR_1(index, vm::VMObject, argValue);

        return argValue;
    }
};

template <>
struct GetArgumentImpl<vm::VMArray>
{
    auto operator()(int index, sdk::Params& params)
    {
        HYP_SCRIPT_GET_ARG_PTR_1(index, vm::VMArray, argValue);

        return argValue;
    }
};

template <int Index, class T>
auto GetArgument(sdk::Params& params)
{
    vm::Value* value = params.args[Index];

    auto converted = ConvertScriptObject<T>(params.apiInstance, *value);

    if (!converted.HasValue())
    {
        char buffer[1024];

        std::snprintf(
            buffer,
            sizeof(buffer),
            "Unexpected type for argument at index %d, could not convert to expected type `%s`",
            Index,
            TypeName<T>().Data());

        params.handler->state->ThrowException(
            params.handler->thread,
            vm::Exception(buffer));

        return T {};
    }

    return converted.Get();
}

#pragma endregion

#pragma region Native Script Binding Helper Structs

struct ScriptBindingsBase;
struct ScriptBindingsHolder;

extern ScriptBindingsHolder g_scriptBindings;

struct ScriptBindingsHolder
{
    static constexpr uint32 maxBindings = 256;

    FixedArray<ScriptBindingsBase*, maxBindings> bindings;
    uint32 bindingIndex = 0;

    void AddBinding(ScriptBindingsBase* scriptBindings);

    void GenerateAll(scriptapi2::Context&);
};

struct ScriptBindingsBase
{
    ScriptBindingsBase(TypeId typeId);
    virtual ~ScriptBindingsBase() = default;

    virtual void Generate(scriptapi2::Context&) = 0;
};

#pragma endregion

} // namespace hyperion
