#include <script/vm/VM.hpp>
#include <script/vm/Value.hpp>
#include <script/vm/HeapValue.hpp>
#include <script/vm/VMArray.hpp>
#include <script/vm/VMArraySlice.hpp>
#include <script/vm/VMObject.hpp>
#include <script/vm/VMString.hpp>
#include <script/vm/VMTypeInfo.hpp>

#include <core/object/HypData.hpp>
#include <core/debug/Debug.hpp>
#include <core/Types.hpp>

#include <script/Instructions.hpp>
#include <script/Hasher.hpp>

#include <algorithm>
#include <cstdio>
#include <cinttypes>
#include <mutex>
#include <sstream>

#define HYP_NUMERIC_OPERATION(a, b, oper)                                              \
    do                                                                                 \
    {                                                                                  \
        switch (numericType)                                                         \
        {                                                                              \
        case NT_I8:                                                                \
            result.i = static_cast<int8>(a.i) oper static_cast<int8>(b.i);              \
            result.flags = Number::FLAG_SIGNED | Number::FLAG_8_BIT;                    \
            break;                                                                     \
        case NT_I16:                                                               \
            result.i = static_cast<int16>(a.i) oper static_cast<int16>(b.i);            \
            result.flags = Number::FLAG_SIGNED | Number::FLAG_16_BIT;                   \
            break;                                                                     \
        case NT_I32:                                                               \
            result.i = static_cast<int32>(a.i) oper static_cast<int32>(b.i);           \
            result.flags = Number::FLAG_SIGNED | Number::FLAG_32_BIT;                  \
            break;                                                                     \
        case NT_I64:                                                               \
            result.i = a.i oper b.i;                                                    \
            result.flags = Number::FLAG_SIGNED | Number::FLAG_64_BIT;                   \
            break;                                                                     \
        case NT_U8:                                                                \
            if (a.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.u = static_cast<uint8>(a.i);                                      \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;              \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.u = static_cast<uint8>(a.u);                           \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;              \
            }                                                                          \
            if (b.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.u oper## = static_cast<uint8>(b.i);                    \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;              \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.u oper## = static_cast<uint8>(b.u);                    \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;              \
            }                                                                          \
            break;                                                                     \
        case NT_U16:                                                               \
            if (a.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.u = static_cast<uint16>(a.i);                         \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;             \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.u = static_cast<uint16>(a.u);                         \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;             \
            }                                                                          \
            if (b.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.u oper## = static_cast<uint16>(b.i);                  \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;             \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.u oper## = static_cast<uint16>(b.u);                  \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;             \
            }                                                                          \
            break;                                                                     \
        case NT_U32:                                                               \
            if (a.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.u = static_cast<uint32>(a.i);                         \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;             \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.u = static_cast<uint32>(a.u);                         \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;             \
            }                                                                          \
            if (b.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.u oper## = static_cast<uint32>(b.i);                  \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;             \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.u oper## = static_cast<uint32>(b.u);                  \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;             \
            }                                                                          \
            break;                                                                     \
        case NT_U64:                                                               \
            if (a.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.u = static_cast<uint64>(a.i);                         \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;             \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.u = a.u;                                              \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;             \
            }                                                                          \
            if (b.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.u oper## = static_cast<uint64>(b.i);                  \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;             \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.u oper## = b.u;                                       \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;             \
            }                                                                          \
            break;                                                                     \
        case NT_F32:                                                               \
            if (a.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.f = static_cast<float>(a.i);                            \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_32_BIT;            \
            }                                                                          \
            else if (a.flags & Number::FLAG_UNSIGNED)                                  \
            {                                                                          \
                result.f = static_cast<float>(a.u);                            \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_32_BIT;            \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.f = static_cast<float>(a.f);                            \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_32_BIT;            \
            }                                                                          \
            if (b.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.f oper## = static_cast<float>(b.i);                     \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_32_BIT;            \
            }                                                                          \
            else if (a.flags & Number::FLAG_UNSIGNED)                                  \
            {                                                                          \
                result.f oper## = static_cast<float>(b.u);                     \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_32_BIT;            \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.f oper## = static_cast<float>(b.f);                     \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_32_BIT;            \
            }                                                                          \
            break;                                                                     \
        case NT_F64:                                                               \
            if (a.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.f = static_cast<double>(a.i);                           \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_64_BIT;            \
            }                                                                          \
            else if (a.flags & Number::FLAG_UNSIGNED)                                  \
            {                                                                          \
                result.f = static_cast<double>(a.u);                           \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_64_BIT;            \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.f = a.f;                                                \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_64_BIT;            \
            }                                                                          \
            if (b.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.f oper## = static_cast<double>(b.i);                    \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_64_BIT;            \
            }                                                                          \
            else if (a.flags & Number::FLAG_UNSIGNED)                                  \
            {                                                                          \
                result.f oper## = static_cast<double>(b.u);                    \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_64_BIT;            \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.f oper## = b.f;                                                  \
                result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_64_BIT;            \
            }                                                                          \
            break;                                                                     \
        default:                                                                       \
            Assert(false, "Invalid type, should not reach this state.");               \
            break;                                                                     \
        }                                                                              \
    }                                                                                  \
    while (0)

#define HYP_NUMERIC_OPERATION_BITWISE(a, b, oper)                                      \
    do                                                                                 \
    {                                                                                  \
        switch (numericType)                                                         \
        {                                                                              \
        case NT_I8:                                                                \
            result.i = static_cast<int8>(a.i) oper static_cast<int8>(b.i);    \
            result.flags = Number::FLAG_SIGNED | Number::FLAG_8_BIT;                    \
            break;                                                                     \
        case NT_I16:                                                               \
            result.i = static_cast<int16>(a.i) oper static_cast<int16>(b.i); \
            result.flags = Number::FLAG_SIGNED | Number::FLAG_16_BIT;                   \
            break;                                                                     \
        case NT_I32:                                                               \
            result.i = static_cast<int32>(a.i) oper static_cast<int32>(b.i); \
            result.flags = Number::FLAG_SIGNED | Number::FLAG_32_BIT;                   \
            break;                                                                     \
        case NT_I64:                                                               \
            result.i = a.i oper b.i;                                         \
            result.flags = Number::FLAG_SIGNED | Number::FLAG_64_BIT;                   \
            break;                                                                     \
        case NT_U8:                                                                \
            if (a.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.u = static_cast<uint8>(a.i);                           \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;              \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.u = static_cast<uint8>(a.u);                           \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;              \
            }                                                                          \
            if (b.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.u oper## = static_cast<uint8>(b.i);                    \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;              \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.u oper## = static_cast<uint8>(b.u);                    \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;              \
            }                                                                          \
            break;                                                                     \
        case NT_U16:                                                               \
            if (a.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.u = static_cast<uint16>(a.i);                         \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;             \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.u = static_cast<uint16>(a.u);                         \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;             \
            }                                                                          \
            if (b.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.u oper## = static_cast<uint16>(b.i);                  \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;             \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.u oper## = static_cast<uint16>(b.u);                  \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;             \
            }                                                                          \
            break;                                                                     \
        case NT_U32:                                                               \
            if (a.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.u = static_cast<uint32>(a.i);                         \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;             \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.u = static_cast<uint32>(a.u);                         \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;             \
            }                                                                          \
            if (b.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.u oper## = static_cast<uint32>(b.i);                  \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;             \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.u oper## = static_cast<uint32>(b.u);                  \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;             \
            }                                                                          \
            break;                                                                     \
        case NT_U64:                                                               \
            if (a.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.u = static_cast<uint64>(a.i);                         \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;             \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.u = a.u;                                              \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;             \
            }                                                                          \
            if (b.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.u oper## = static_cast<uint64>(b.i);                  \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;             \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.u oper## = b.u;                                       \
                result.flags = Number::FLAG_UNSIGNED | Number::FLAG_64_BIT;             \
            }                                                                          \
            break;                                                                     \
        default:                                                                       \
            state->ThrowException(thread, Exception::InvalidBitwiseArgument());        \
            break;                                                                     \
        }                                                                              \
    }                                                                                  \
    while (0)

namespace hyperion {
namespace vm {

template <class T, typename = std::enable_if_t<!std::is_same_v<Script_VMData, NormalizedType<T>> && !std::is_same_v<Number, NormalizedType<T>>>>
static inline Value MakeScriptValue(T&& data)
{
    return Value(HypData(std::forward<T>(data)));
}

static inline Value MakeScriptValue(const Script_VMData& data)
{
    return Value(data);
}

static inline Value MakeScriptValue(const Number& number)
{
    return Value(number);
}

static inline Value MakeScriptValueRef(Value* pRef)
{
    Assert(pRef != nullptr);

    Script_VMData vmData;
    vmData.type = Script_VMData::VALUE_REF;
    vmData.valueRef = pRef;

    return Value(vmData);
}

class InstructionHandler
{
public:
    VMState* state;
    Script_ExecutionThread* thread;
    BytecodeStream* bs;

    InstructionHandler(
        VMState* state,
        Script_ExecutionThread* thread,
        BytecodeStream* bs)
        : state(state),
          thread(thread),
          bs(bs)
    {
    }

    HYP_FORCE_INLINE void StoreStaticString(uint32 len, const char* str)
    {
        Assert(false, "Not implemented, will be removed soon.");
        // the value will be freed on
        // the destructor call of state->m_staticMemory
        // HeapValue *hv = new HeapValue();
        // hv->Assign(VMString(str));

        // Value sv;
        // sv.m_type = Value::HEAP_POINTER;
        // sv.m_value.internal.ptr = hv;

        // state->m_staticMemory.Store(std::move(sv));
    }

    HYP_FORCE_INLINE void StoreStaticAddress(BCAddress addr)
    {
        Assert(false, "Not implemented, will be removed soon.");
        // Value sv;
        // sv.m_type = Value::ADDRESS;
        // sv.m_value.addr = addr;

        // state->m_staticMemory.Store(std::move(sv));
    }

    HYP_FORCE_INLINE void StoreStaticFunction(
        BCAddress addr,
        uint8 nargs,
        uint8 flags)
    {
        Assert(false, "Not implemented, will be removed soon.");
        // Value sv;
        // sv.m_type = Value::FUNCTION;
        // sv.m_value.internal.func.m_addr = addr;
        // sv.m_value.internal.func.m_nargs = nargs;
        // sv.m_value.internal.func.m_flags = flags;

        // state->m_staticMemory.Store(std::move(sv));
    }

    HYP_FORCE_INLINE void StoreStaticType(
        const char* typeName,
        uint16 size,
        char** names)
    {
        Assert(false, "Not implemented, will be removed soon.");
        // the value will be freed on
        // the destructor call of state->m_staticMemory
        // HeapValue *hv = new HeapValue();
        // hv->Assign(VMTypeInfo(typeName, size, names));

        // Value sv;
        // sv.m_type = Value::HEAP_POINTER;
        // sv.m_value.internal.ptr = hv;
        // state->m_staticMemory.Store(std::move(sv));
    }

    HYP_FORCE_INLINE void LoadI32(BCRegister reg, int32 i32)
    {
        thread->m_regs[reg].AssignValue(MakeScriptValue(i32), false);
    }

    HYP_FORCE_INLINE void LoadI64(BCRegister reg, int64 i64)
    {
        thread->m_regs[reg].AssignValue(MakeScriptValue(i64), false);
    }

    HYP_FORCE_INLINE void LoadU32(BCRegister reg, uint32 u32)
    {
        thread->m_regs[reg].AssignValue(MakeScriptValue(u32), false);
    }

    HYP_FORCE_INLINE void LoadU64(BCRegister reg, uint64 u64)
    {
        thread->m_regs[reg].AssignValue(MakeScriptValue(u64), false);
    }

    HYP_FORCE_INLINE void LoadF32(BCRegister reg, float f32)
    {
        thread->m_regs[reg].AssignValue(MakeScriptValue(f32), false);
    }

    HYP_FORCE_INLINE void LoadF64(BCRegister reg, double f64)
    {
        thread->m_regs[reg].AssignValue(MakeScriptValue(f64), false);
    }

    HYP_FORCE_INLINE void LoadOffset(BCRegister reg, uint16 offset)
    {
        Script_StackMemory& stk = state->MAIN_THREAD->m_stack;

        Assert(
            offset <= stk.GetStackPointer(),
            "Stack offset out of bounds (%u)",
            offset);

        // read value from stack at (sp - offset)
        // into the the register
        thread->m_regs[reg].AssignValue(MakeScriptValueRef(&thread->m_stack[thread->m_stack.GetStackPointer() - offset]), false);
    }

    HYP_FORCE_INLINE void LoadIndex(BCRegister reg, uint16 index)
    {
        Script_StackMemory& stk = state->MAIN_THREAD->m_stack;

        Assert(
            index < stk.GetStackPointer(),
            "Stack index out of bounds (%u >= %llu)",
            index,
            stk.GetStackPointer());

        // read value from stack at the index into the the register
        // NOTE: read from main thread
        thread->m_regs[reg].AssignValue(MakeScriptValueRef(&stk[index]), false);
    }

    HYP_FORCE_INLINE void LoadStatic(BCRegister reg, uint16 index)
    {
        // read value from static memory
        // at the index into the the register
        thread->m_regs[reg].AssignValue(MakeScriptValueRef(&state->m_staticMemory[index]), false);
    }

    HYP_FORCE_INLINE void LoadConstantString(BCRegister reg, uint32 len, const char* str)
    {
        if (HeapValue* hv = state->HeapAlloc(thread))
        {
            hv->Assign(VMString(str));

            thread->m_regs[reg].AssignValue(MakeScriptValue(VMString(str)), false);

            hv->Mark();
        }
    }

    HYP_FORCE_INLINE void LoadAddr(BCRegister reg, BCAddress addr)
    {
        Script_VMData vmData;
        vmData.type = Script_VMData::ADDRESS;
        vmData.addr = addr;

        thread->m_regs[reg].AssignValue(MakeScriptValue(vmData), false);
    }

    HYP_FORCE_INLINE void LoadFunc(
        BCRegister reg,
        BCAddress addr,
        uint8 nargs,
        uint8 flags)
    {
        Script_VMData vmData;
        vmData.type = Script_VMData::FUNCTION;
        vmData.func.m_addr = addr;
        vmData.func.m_nargs = nargs;
        vmData.func.m_flags = flags;

        thread->m_regs[reg].AssignValue(MakeScriptValue(vmData), false);
    }

    HYP_FORCE_INLINE void LoadType( // come back to this
        BCRegister reg,
        uint16 typeNameLen,
        const char* typeName,
        uint16 size,
        char** names)
    {
        // create members
        Member* members = new Member[size];

        for (uint16 i = 0; i < size; i++)
        {
            Memory::StrCpy(members[i].name, names[i], sizeof(Member::name));
            members[i].hash = hashFnv1(names[i]);
            members[i].value = Value();
        }

        Value& parentClassValue = thread->m_regs[reg];

        // create prototype object
        Value value = MakeScriptValue(VMObject(members, size, std::move(parentClassValue)));

        delete[] members;

        thread->m_regs[reg].AssignValue(std::move(value), false);
    }

    HYP_FORCE_INLINE void LoadMem(BCRegister dst, BCRegister src, uint8 index)
    {
        Value& sv = thread->m_regs[src];

        if (VMObject* object = sv.GetObject())
        {
            Assert(
                index < object->GetSize(),
                "Index out of bounds (%u >= %llu)",
                index,
                object->GetSize());

            thread->m_regs[dst].AssignValue(MakeScriptValueRef(&object->GetMember(index).value), false);

            return;
        }

        state->ThrowException(
            thread,
            Exception("Cannot access member by index: Not an VMObject"));
    }

    HYP_FORCE_INLINE void LoadMemHash(BCRegister dstReg, BCRegister srcReg, uint32 hash)
    {
        Value& sv = thread->m_regs[srcReg];

        if (VMObject* object = sv.GetObject())
        {
            if (Member* member = object->LookupMemberFromHash(hash))
            {
                thread->m_regs[dstReg].AssignValue(MakeScriptValueRef(&member->value), false);
            }
            else
            {
                state->ThrowException(
                    thread,
                    Exception::MemberNotFoundException(hash));
            }
            return;
        }

        state->ThrowException(
            thread,
            Exception("Cannot access member by hash: Not an VMObject"));
    }

    HYP_FORCE_INLINE void LoadArrayIdx(BCRegister dstReg, BCRegister srcReg, BCRegister indexReg)
    {
        Value& sv = thread->m_regs[srcReg];

        struct
        {
            Number index;
        } key;

        if (!thread->m_regs[indexReg].GetSignedOrUnsigned(&key.index))
        {
            state->ThrowException(
                thread,
                Exception("Array index must be of type int or uint32"));

            return;
        }

        if (VMArray* array = sv.GetArray())
        {
            if (key.index.flags & Number::FLAG_SIGNED)
            {
                if (static_cast<SizeType>(key.index.i) >= array->GetSize())
                {
                    state->ThrowException(
                        thread,
                        Exception::OutOfBoundsException());
                    return;
                }

                if (key.index.i < 0)
                {
                    // wrap around (python style)
                    key.index.i = (int64)(array->GetSize() + key.index.i);
                    if (key.index.i < 0 || key.index.i >= array->GetSize())
                    {
                        state->ThrowException(
                            thread,
                            Exception::OutOfBoundsException());
                        return;
                    }
                }

                thread->m_regs[dstReg].AssignValue(MakeScriptValueRef(&array->AtIndex(key.index.i)), false);
            }
            else if (key.index.flags & Number::FLAG_UNSIGNED)
            {
                if (static_cast<SizeType>(key.index.u) >= array->GetSize())
                {
                    state->ThrowException(
                        thread,
                        Exception::OutOfBoundsException());
                    return;
                }

                thread->m_regs[dstReg].AssignValue(MakeScriptValueRef(&array->AtIndex(key.index.u)), false);
            }

            return;
        }

        // throw an exception
        state->ThrowException(thread, Exception("Not an array!"));
    }

    HYP_FORCE_INLINE void LoadOffsetRef(BCRegister reg, uint16 offset)
    {
        thread->m_regs[reg] = MakeScriptValueRef(&thread->m_stack[thread->m_stack.GetStackPointer() - offset]);
    }

    HYP_FORCE_INLINE void LoadIndexRef(BCRegister reg, uint16 index)
    {
        auto& stk = state->MAIN_THREAD->m_stack;

        Assert(
            index < stk.GetStackPointer(),
            "Stack index out of bounds (%u >= %llu)",
            index,
            stk.GetStackPointer());

        thread->m_regs[reg] = MakeScriptValueRef(&stk[index]);
    }

    HYP_FORCE_INLINE void LoadRef(BCRegister dstReg, BCRegister srcReg)
    {
        thread->m_regs[dstReg] = MakeScriptValueRef(&thread->m_regs[srcReg]);
    }

    HYP_FORCE_INLINE void LoadDeref(BCRegister dstReg, BCRegister srcReg)
    {
        Value& src = thread->m_regs[srcReg];
        Value* pRef = src.GetRef();

        Assert(pRef != nullptr, "Invalid reference!");

        thread->m_regs[dstReg].AssignValue(MakeScriptValueRef(pRef), false);
    }

    HYP_FORCE_INLINE void LoadNull(BCRegister reg)
    {
        thread->m_regs[reg].AssignValue(Value(), false);
    }

    HYP_FORCE_INLINE void LoadTrue(BCRegister reg)
    {
        thread->m_regs[reg].AssignValue(MakeScriptValue(true), false);
    }

    HYP_FORCE_INLINE void LoadFalse(BCRegister reg)
    {
        thread->m_regs[reg].AssignValue(MakeScriptValue(false), false);
    }

    HYP_FORCE_INLINE void MovOffset(uint16 offset, BCRegister reg)
    {
        // copy value from register to stack value at (sp - offset)
        thread->m_stack[thread->m_stack.GetStackPointer() - offset].AssignValue(std::move(thread->m_regs[reg]), true);
    }

    HYP_FORCE_INLINE void MovIndex(uint16 index, BCRegister reg)
    {
        // copy value from register to stack value at index
        state->MAIN_THREAD->m_stack[index].AssignValue(std::move(thread->m_regs[reg]), true);
    }

    HYP_FORCE_INLINE void MovStatic(uint16 index, BCRegister reg)
    {
        Assert(index < state->m_staticMemory.staticSize);

        //// ensure we will not be overwriting something that is marked ALWAYS_ALIVE
        //// will cause a memory leak if we overwrite it, or if we did change the flag,
        //// it could cause corruption in places where we expect something to always exist.
        //if (state->m_staticMemory[index].m_type == Value::HEAP_POINTER)
        //{
        //    HeapValue* hv = state->m_staticMemory[index].m_value.internal.ptr;
        //    if (hv != nullptr && (hv->GetFlags() & GC_ALWAYS_ALIVE))
        //    {
        //        // state->ThrowException(
        //        //     thread,
        //        //     Exception("Cannot overwrite static value marked as ALWAYS_ALIVE")
        //        // );

        //        // return;

        //        // TEMP, hack
        //        hv->DisableFlags(GC_ALWAYS_ALIVE);
        //    }
        //}

        //// Mark our new value as ALWAYS_ALIVE if applicable.
        //if (thread->m_regs[reg].m_type == Value::HEAP_POINTER)
        //{
        //    if (HeapValue* hv = thread->m_regs[reg].m_value.internal.ptr)
        //    {
        //        thread->m_regs[reg].m_value.internal.ptr->EnableFlags(GC_ALWAYS_ALIVE);
        //    }
        //}

        // @TODO: Come back to review this for GC stuff.

        // copy value from register to static memory at index
        // moves to static do not impact refs
        state->m_staticMemory[index].AssignValue(std::move(thread->m_regs[reg]), false);
    }

    HYP_FORCE_INLINE void MovMem(BCRegister dstReg, uint8 index, BCRegister srcReg)
    {
        Value& sv = thread->m_regs[dstReg];
        
        VMObject* object = sv.GetObject();
        if (object == nullptr)
        {
            state->ThrowException(
                thread,
                Exception("Cannot assign member by index: Not an VMObject"));
            return;
        }

        if (index >= object->GetSize())
        {
            state->ThrowException(
                thread,
                Exception::OutOfBoundsException());
            return;
        }

        object->GetMember(index).value.AssignValue(std::move(thread->m_regs[srcReg]), true);
        object->GetMember(index).value.Mark();
    }

    HYP_FORCE_INLINE void MovMemHash(BCRegister dstReg, uint32_t hash, BCRegister srcReg)
    {
        Value& sv = thread->m_regs[dstReg];

        VMObject* object = sv.GetObject();
        if (object == nullptr)
        {
            state->ThrowException(
                thread,
                Exception("Cannot assign member by hash: Not an VMObject"));
            return;
        }

        Member* member = object->LookupMemberFromHash(hash);
        if (member == nullptr)
        {
            state->ThrowException(
                thread,
                Exception::MemberNotFoundException(hash));
            return;
        }

        // set value in member
        member->value.AssignValue(std::move(thread->m_regs[srcReg]), true);
        member->value.Mark();
    }

    HYP_FORCE_INLINE void MovArrayIdx(BCRegister dstReg, uint32 index, BCRegister srcReg)
    {
        Value& sv = thread->m_regs[dstReg];

        VMArray* array = sv.GetArray();

        //if (array == nullptr)
        //{
        //    if (auto* memoryBuffer = hv->GetPointer<VMMemoryBuffer>())
        //    {
        //        if (static_cast<SizeType>(index) >= memoryBuffer->GetSize())
        //        {
        //            state->ThrowException(
        //                thread,
        //                Exception::OutOfBoundsException());

        //            return;
        //        }

        //        if (index < 0)
        //        {
        //            // wrap around (python style)
        //            index = static_cast<int64>(memoryBuffer->GetSize() + static_cast<SizeType>(index));
        //            if (index < 0 || static_cast<SizeType>(index) >= memoryBuffer->GetSize())
        //            {
        //                state->ThrowException(
        //                    thread,
        //                    Exception::OutOfBoundsException());
        //                return;
        //            }
        //        }

        //        Number dstData;

        //        if (!thread->m_regs[srcReg].GetSignedOrUnsigned(&dstData))
        //        {
        //            state->ThrowException(
        //                thread,
        //                Exception::InvalidArgsException("integer"));

        //            return;
        //        }

        //        ubyte byteValue = 0x0;

        //        // take the passed int/uint value and clip it to the first byte.
        //        if (dstData.flags & Number::FLAG_SIGNED)
        //        {
        //            // convert -128..127 to 0..255 for signed integers.
        //            byteValue = static_cast<ubyte>((dstData.i + 128) & 0xff);
        //        }
        //        else if (dstData.flags & Number::FLAG_UNSIGNED)
        //        {
        //            byteValue = static_cast<ubyte>(dstData.u & 0xff);
        //        }
        //        else
        //        {
        //            Assert(false, "Should not reach!");
        //        }

        //        // copy first byte from value
        //        Memory::MemCpy(static_cast<ubyte*>(memoryBuffer->GetBuffer()) + index, &byteValue, sizeof(ubyte));

        //        return;
        //    }

        //    char buffer[256];
        //    std::snprintf(
        //        buffer,
        //        sizeof(buffer),
        //        "Expected Array or MemoryBuffer, got %s",
        //        sv.GetTypeString());

        //    state->ThrowException(
        //        thread,
        //        Exception(buffer));

        //    return;
        //}

        if (array != nullptr)
        {
            if (index >= array->GetSize())
            {
                state->ThrowException(
                    thread,
                    Exception::OutOfBoundsException());
                return;
            }

            /*if (index < 0) {
                // wrap around (python style)
                index = array->GetSize() + index;
                if (index < 0 || index >= array->GetSize()) {
                    state->ThrowException(
                        thread,
                        Exception::OutOfBoundsException()
                    );
                    return;
                }
            }*/

            array->AtIndex(index).AssignValue(std::move(thread->m_regs[srcReg]), false);
            array->AtIndex(index).Mark();
            return;
        }

        // not an Array
        state->ThrowException(thread, Exception("Not an array!"));
    }

    HYP_FORCE_INLINE void MovArrayIdxReg(BCRegister dstReg, BCRegister indexReg, BCRegister srcReg)
    {
        Value& sv = thread->m_regs[dstReg];

        VMArray* array = sv.GetArray();

        Number index;
        Value& indexRegisterValue = thread->m_regs[indexReg];

        if (!indexRegisterValue.GetSignedOrUnsigned(&index))
        {
            state->ThrowException(
                thread,
                Exception::InvalidArgsException("integer"));

            return;
        }

        //if (array == nullptr)
        //{
        //    if (auto* memoryBuffer = hv->GetPointer<VMMemoryBuffer>())
        //    {
        //        Number dstData;

        //        if (!thread->m_regs[srcReg].GetSignedOrUnsigned(&dstData))
        //        {
        //            state->ThrowException(
        //                thread,
        //                Exception::InvalidArgsException("integer"));

        //            return;
        //        }

        //        if (index.flags & Number::FLAG_SIGNED)
        //        {
        //            int64 indexValue = index.i;

        //            if (static_cast<SizeType>(indexValue) >= memoryBuffer->GetSize())
        //            {
        //                state->ThrowException(
        //                    thread,
        //                    Exception::OutOfBoundsException());

        //                return;
        //            }

        //            if (indexValue < 0)
        //            {
        //                // wrap around (python style)
        //                indexValue = static_cast<int64>(memoryBuffer->GetSize() + static_cast<SizeType>(indexValue));
        //                if (indexValue < 0 || static_cast<SizeType>(indexValue) >= memoryBuffer->GetSize())
        //                {
        //                    state->ThrowException(
        //                        thread,
        //                        Exception::OutOfBoundsException());
        //                    return;
        //                }
        //            }

        //            Memory::MemCpy(&static_cast<uint8*>(memoryBuffer->GetBuffer())[indexValue], &dstData, sizeof(dstData));
        //        }
        //        else
        //        { // unsigned
        //            const uint64 indexValue = index.u;

        //            if (static_cast<SizeType>(indexValue) >= memoryBuffer->GetSize())
        //            {
        //                state->ThrowException(
        //                    thread,
        //                    Exception::OutOfBoundsException());

        //                return;
        //            }

        //            Memory::MemCpy(&static_cast<uint8*>(memoryBuffer->GetBuffer())[indexValue], &dstData, sizeof(dstData));
        //        }

        //        return;
        //    }

        //    char buffer[256];
        //    std::snprintf(
        //        buffer,
        //        sizeof(buffer),
        //        "Expected Array or MemoryBuffer, got %s",
        //        indexRegisterValue.GetTypeString());

        //    state->ThrowException(
        //        thread,
        //        Exception(buffer));

        //    return;
        //}

        if (array != nullptr)
        {
            if (index.flags & Number::FLAG_SIGNED)
            {
                int64 indexValue = index.i;

                if (static_cast<SizeType>(indexValue) >= array->GetSize())
                {
                    state->ThrowException(
                        thread,
                        Exception::OutOfBoundsException());

                    return;
                }

                if (indexValue < 0)
                {
                    // wrap around (python style)
                    indexValue = static_cast<int64>(array->GetSize() + static_cast<SizeType>(indexValue));
                    if (indexValue < 0 || static_cast<SizeType>(indexValue >= array->GetSize()))
                    {
                        state->ThrowException(
                            thread,
                            Exception::OutOfBoundsException());
                        return;
                    }
                }

                array->AtIndex(indexValue).AssignValue(std::move(thread->m_regs[srcReg]), false);
                array->AtIndex(indexValue).Mark();
            }
            else
            { // unsigned
                const uint64 indexValue = index.u;

                if (static_cast<SizeType>(indexValue) >= array->GetSize())
                {
                    state->ThrowException(
                        thread,
                        Exception::OutOfBoundsException());

                    return;
                }

                array->AtIndex(indexValue).AssignValue(std::move(thread->m_regs[srcReg]), false);
                array->AtIndex(indexValue).Mark();
            }

            return;
        }

        state->ThrowException(thread, Exception("Not an array!"));
    }

    HYP_FORCE_INLINE void MovReg(BCRegister dstReg, BCRegister srcReg)
    {
        thread->m_regs[dstReg] = std::move(thread->m_regs[srcReg]);
    }

    HYP_FORCE_INLINE void HasMemHash(BCRegister dstReg, BCRegister srcReg, uint32 hash)
    {
        Value& src = thread->m_regs[srcReg];

        Value& result = thread->m_regs[dstReg];

        if (VMObject* object = src.GetObject())
        {
            result.AssignValue(MakeScriptValue(object->LookupMemberFromHash(hash) != nullptr), false);
        }
        else
        {
            state->ThrowException(thread, Exception("Not an object!"));
        }
    }

    HYP_FORCE_INLINE void Push(BCRegister reg)
    {
        // push a copy of the register value to the top of the stack
        thread->m_stack.Push(std::move(thread->m_regs[reg]));
    }

    HYP_FORCE_INLINE void Pop()
    {
        thread->m_stack.Pop();
    }

    HYP_FORCE_INLINE void PushArray(BCRegister dstReg, BCRegister srcReg)
    {
        Value& dst = thread->m_regs[dstReg];
        
        VMArray* array = dst.GetArray();
        if (!array)
        {
            state->ThrowException(
                thread,
                Exception("Not an Array"));
            return;
        }

        array->Push(std::move(thread->m_regs[srcReg]));
        array->AtIndex(array->GetSize() - 1).Mark();
    }

    HYP_FORCE_INLINE void AddSp(uint16 n)
    {
        thread->m_stack.m_sp += n;
    }

    HYP_FORCE_INLINE void SubSp(uint16 n)
    {
        thread->m_stack.m_sp -= n;
    }

    HYP_FORCE_INLINE void Jmp(BCAddress addr)
    {
        bs->Seek(addr);
    }

    HYP_FORCE_INLINE void Je(BCAddress addr)
    {
        if (thread->m_regs.m_flags & EQUAL)
        {
            bs->Seek(addr);
        }
    }

    HYP_FORCE_INLINE void Jne(BCAddress addr)
    {
        if (!(thread->m_regs.m_flags & EQUAL))
        {
            bs->Seek(addr);
        }
    }

    HYP_FORCE_INLINE void Jg(BCAddress addr)
    {
        if (thread->m_regs.m_flags & GREATER)
        {
            bs->Seek(addr);
        }
    }

    HYP_FORCE_INLINE void Jge(BCAddress addr)
    {
        if (thread->m_regs.m_flags & (GREATER | EQUAL))
        {
            bs->Seek(addr);
        }
    }

    HYP_FORCE_INLINE void Call(BCRegister reg, uint8_t nargs)
    {
        state->m_vm->Invoke(this, std::move(thread->m_regs[reg]), nargs);
    }

    HYP_FORCE_INLINE void Ret()
    {
        // get top of stack (should be the address before jumping)
        Value& top = thread->GetStack().Top();

        Script_VMData* vmData = top.GetVMData();
        Assert(vmData != nullptr);
        Assert(vmData->type == Script_VMData::FUNCTION);

        auto& callInfo = vmData->call;

        // leave function and return to previous position
        bs->Seek(callInfo.returnAddress);

        // increase stack size by the amount required by the call
        thread->GetStack().m_sp += callInfo.varargsPush - 1;
        // NOTE: the -1 is because we will be popping the FUNCTION_CALL
        // object from the stack anyway...

        // decrease function depth
        thread->m_funcDepth--;
    }

    HYP_FORCE_INLINE void BeginTry(BCAddress addr)
    {
        ++thread->m_exceptionState.m_tryCounter;

        // increase stack size to store data about this try block
        Script_VMData vmData;
        vmData.type = Script_VMData::TRY_CATCH_INFO;
        vmData.tryCatchInfo.catchAddress = addr;

        // store the info
        thread->m_stack.Push(MakeScriptValue(vmData));
    }

    HYP_FORCE_INLINE void EndTry()
    {
        // pop the try catch info from the stack
        Value& top = thread->m_stack.Top();

        Script_VMData* vmData = top.GetVMData();
        Assert(vmData != nullptr);
        Assert(vmData->type == Script_VMData::TRY_CATCH_INFO);

        Assert(thread->m_exceptionState.m_tryCounter != 0);

        // pop try catch info
        thread->m_stack.Pop();
        --thread->m_exceptionState.m_tryCounter;
    }

    HYP_FORCE_INLINE void New(BCRegister dst, BCRegister src) // come back to this
    {
        // read value from register
        Value& classValue = thread->m_regs[src];

        Array<Span<Member>> memberSpans;

        VMObject* classPtr = classValue.GetObject();

        while (classPtr != nullptr)
        {
            // the NEW instruction makes a copy of the $proto data member
            // of the prototype object.
            Member* protoMem = classPtr->LookupMemberFromHash(VMObject::PROTO_MEMBER_HASH, false);

            if (!protoMem)
            {
                // This base class does not have a prototype member.
                break;
            }

            VMObject* protoMemberObject = protoMem->value.GetObject();

            if (!protoMemberObject)
            {
                state->ThrowException(
                    thread,
                    Exception::InvalidConstructorException());

                return;
            }

            memberSpans.PushBack({ protoMemberObject->GetMembers(), protoMemberObject->GetSize() });

            Member* baseMember = classPtr->LookupMemberFromHash(VMObject::BASE_MEMBER_HASH, false);

            if (baseMember)
            {
                classPtr = baseMember->value.GetObject();
            }
            else
            {
                classPtr = nullptr;
            }
        }

        // Combine all proto members
        // The topmost type (first in the chain)
        // MUST be first so that loads/stores using member index match up!
        Array<Member> allMembers;
        allMembers.Reserve(1);

        for (auto& it : memberSpans)
        {
            for (SizeType index = 0; index < it.Size(); index++)
            {
                allMembers.PushBack(it.Data()[index]);
            }
        }

        thread->m_regs[dst].AssignValue(MakeScriptValue(VMObject(allMembers.Data(), allMembers.Size(), std::move(classValue))), false);
    }

    HYP_FORCE_INLINE void NewArray(BCRegister dst, uint32_t size)
    {
        // assign register value to the allocated object
        Value& value = thread->m_regs[dst];
        value = MakeScriptValue(VMArray());
    }

    HYP_FORCE_INLINE void Cmp(BCRegister lhsReg, BCRegister rhsReg)
    {
        // dropout early for comparing something against itself
        if (lhsReg == rhsReg)
        {
            thread->m_regs.m_flags = EQUAL;
            return;
        }

        // load values from registers
        Value* lhs = &thread->m_regs[lhsReg];
        Value* rhs = &thread->m_regs[rhsReg];

        Number a, b;

        if (lhs->GetSignedOrUnsigned(&a) && rhs->GetSignedOrUnsigned(&b))
        {
            if ((a.flags & Number::FLAG_SIGNED) && (b.flags & Number::FLAG_SIGNED))
            {
                thread->m_regs.m_flags = (a.i == b.i)
                    ? EQUAL
                    : ((a.i > b.i)
                              ? GREATER
                              : NONE);
            }
            else if ((a.flags & Number::FLAG_SIGNED) && (b.flags & Number::FLAG_UNSIGNED))
            {
                thread->m_regs.m_flags = (a.i == b.u)
                    ? EQUAL
                    : ((a.i > b.u)
                              ? GREATER
                              : NONE);
            }
            else if ((a.flags & Number::FLAG_UNSIGNED) && (b.flags & Number::FLAG_SIGNED))
            {
                thread->m_regs.m_flags = (a.u == b.i)
                    ? EQUAL
                    : ((a.u > b.i)
                              ? GREATER
                              : NONE);
            }
            else if ((a.flags & Number::FLAG_UNSIGNED) && (b.flags & Number::FLAG_UNSIGNED))
            {
                thread->m_regs.m_flags = (a.u == b.u)
                    ? EQUAL
                    : ((a.u > b.u)
                              ? GREATER
                              : NONE);
            }
        }
        else if (lhs->GetNumber(&a.f) && rhs->GetNumber(&b.f))
        {
            thread->m_regs.m_flags = (a.f == b.f)
                ? EQUAL
                : ((a.f > b.f)
                          ? GREATER
                          : NONE);
        }
        else
        {
            bool lhsBool;
            bool rhsBool;

            if (lhs->GetBoolean(&lhsBool) && rhs->GetBoolean(&rhsBool))
            {
                thread->m_regs.m_flags = (lhsBool == rhsBool)
                    ? EQUAL
                    : ((lhsBool > rhsBool)
                        ? GREATER
                        : NONE);
            }
            else
            {
                const int res = Value::CompareAsPointers(lhs, rhs);

                if (res != -1)
                {
                    thread->m_regs.m_flags = res;
                }
                else
                {
                    state->ThrowException(
                        thread,
                        Exception::InvalidComparisonException(
                            lhs->GetTypeString(),
                            rhs->GetTypeString()));
                }
            }
        }
    }

    HYP_FORCE_INLINE void CmpZ(BCRegister reg)
    {
        // load values from registers
        Value* lhs = &thread->m_regs[reg];

        Number num;

        if (lhs->GetSignedOrUnsigned(&num))
        {
            thread->m_regs.m_flags = ((num.flags & Number::FLAG_SIGNED) ? !num.i : !num.u) ? EQUAL : NONE;
        }
        else if (lhs->GetFloatingPoint(&num.f))
        {
            thread->m_regs.m_flags = !num.f ? EQUAL : NONE;
        }
        else
        {
            bool boolValue;
            if (lhs->GetBoolean(&boolValue))
            {
                thread->m_regs.m_flags = !boolValue ? EQUAL : NONE;
            }
            else
            {
                void* ptrValue = lhs->ToRef().GetPointer();

                thread->m_regs.m_flags = !ptrValue ? EQUAL : NONE;
            }
        }
    }

    HYP_FORCE_INLINE void Add(
        BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Value* lhs = &thread->m_regs[lhsReg];
        Value* rhs = &thread->m_regs[rhsReg];
        
        const NumericType numericType = MATCH_TYPES(lhs->GetNumericType(), rhs->GetNumericType());

        Number a, b;
        Number result { numericType };

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            HYP_NUMERIC_OPERATION(a, b, +);
        }
        else
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "ADD",
                    lhs->GetTypeString(),
                    rhs->GetTypeString()));
            return;
        }

        // set the destination register to be the result
        thread->m_regs[dstReg] = MakeScriptValue(result);
    }

    HYP_FORCE_INLINE void Sub(
        BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Value* lhs = &thread->m_regs[lhsReg];
        Value* rhs = &thread->m_regs[rhsReg];
        
        const NumericType numericType = MATCH_TYPES(lhs->GetNumericType(), rhs->GetNumericType());

        Number a, b;
        Number result { numericType };

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            HYP_NUMERIC_OPERATION(a, b, -);
        }
        else
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "SUB",
                    lhs->GetTypeString(),
                    rhs->GetTypeString()));
            return;
        }

        // set the destination register to be the result
        thread->m_regs[dstReg] = MakeScriptValue(result);
    }

    HYP_FORCE_INLINE void Mul(
        BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Value* lhs = &thread->m_regs[lhsReg];
        Value* rhs = &thread->m_regs[rhsReg];
        
        const NumericType numericType = MATCH_TYPES(lhs->GetNumericType(), rhs->GetNumericType());

        Number a, b;
        Number result { numericType };

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            HYP_NUMERIC_OPERATION(a, b, *);
        }
        else
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "MUL",
                    lhs->GetTypeString(),
                    rhs->GetTypeString()));
            return;
        }

        // set the destination register to be the result
        thread->m_regs[dstReg] = MakeScriptValue(result);
    }

    HYP_FORCE_INLINE void Div(
        BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Value* lhs = &thread->m_regs[lhsReg];
        Value* rhs = &thread->m_regs[rhsReg];
        
        const NumericType numericType = MATCH_TYPES(lhs->GetNumericType(), rhs->GetNumericType());

        Number a, b;
        Number result { numericType };

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            if ((b.flags & Number::FLAG_SIGNED) && b.i == 0)
            {
                state->ThrowException(thread, Exception::DivisionByZeroException());
                return;
            }
            else if ((b.flags & Number::FLAG_UNSIGNED) && b.u == 0)
            {
                state->ThrowException(thread, Exception::DivisionByZeroException());
                return;
            }

            HYP_NUMERIC_OPERATION(a, b, /);
        }
        else
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "DIV",
                    lhs->GetTypeString(),
                    rhs->GetTypeString()));
            return;
        }

        // set the destination register to be the result
        thread->m_regs[dstReg] = MakeScriptValue(result);
    }

    HYP_FORCE_INLINE void Mod(
        BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Value* lhs = &thread->m_regs[lhsReg];
        Value* rhs = &thread->m_regs[rhsReg];
        
        const NumericType numericType = MATCH_TYPES(lhs->GetNumericType(), rhs->GetNumericType());

        Number a, b;
        Number result { numericType };

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            // custom handling for mod to allow floats to work
            if ((b.flags & Number::FLAG_SIGNED) && b.i == 0)
            {
                state->ThrowException(thread, Exception::DivisionByZeroException());
                return;
            }
            else if ((b.flags & Number::FLAG_UNSIGNED) && b.u == 0)
            {
                state->ThrowException(thread, Exception::DivisionByZeroException());
                return;
            }

            if (a.flags & Number::FLAG_FLOATING_POINT || b.flags & Number::FLAG_FLOATING_POINT)
            {
                // at least one operand is a float, do floating point mod
                result.f = std::fmod(a.f, b.f);
                result.flags = Number::FLAG_FLOATING_POINT;
            }
            else if (a.flags & Number::FLAG_SIGNED && b.flags & Number::FLAG_SIGNED)
            {
                result.i = a.i % b.i;
                result.flags = Number::FLAG_SIGNED;
            }
            else if (a.flags & Number::FLAG_SIGNED && b.flags & Number::FLAG_UNSIGNED)
            {
                result.i = a.i % static_cast<int64>(b.u);
                result.flags = Number::FLAG_SIGNED;
            }
            else if (a.flags & Number::FLAG_UNSIGNED && b.flags & Number::FLAG_SIGNED)
            {
                result.u = a.u % static_cast<uint64>(b.i);
                result.flags = Number::FLAG_UNSIGNED;
            }
            else if (a.flags & Number::FLAG_UNSIGNED && b.flags & Number::FLAG_UNSIGNED)
            {
                result.u = a.u % b.u;
                result.flags = Number::FLAG_UNSIGNED;
            }
            else
            {
                HYP_UNREACHABLE();
            }
        }
        else
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "MOD",
                    lhs->GetTypeString(),
                    rhs->GetTypeString()));
            return;
        }

        // set the destination register to be the result
        thread->m_regs[dstReg] = MakeScriptValue(result);
    }

    HYP_FORCE_INLINE void And(
        BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Value* lhs = &thread->m_regs[lhsReg];
        Value* rhs = &thread->m_regs[rhsReg];
        
        const NumericType numericType = MATCH_TYPES(lhs->GetNumericType(), rhs->GetNumericType());

        Number a, b;
        Number result { numericType };

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            HYP_NUMERIC_OPERATION_BITWISE(a, b, &);
        }
        else
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "AND",
                    lhs->GetTypeString(),
                    rhs->GetTypeString()));
            return;
        }

        // set the destination register to be the result
        thread->m_regs[dstReg] = MakeScriptValue(result);
    }

    HYP_FORCE_INLINE void Or(
        BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Value* lhs = &thread->m_regs[lhsReg];
        Value* rhs = &thread->m_regs[rhsReg];
        
        const NumericType numericType = MATCH_TYPES(lhs->GetNumericType(), rhs->GetNumericType());

        Number a, b;
        Number result { numericType };

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            HYP_NUMERIC_OPERATION_BITWISE(a, b, |);
        }
        else
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "OR",
                    lhs->GetTypeString(),
                    rhs->GetTypeString()));
            return;
        }

        // set the destination register to be the result
        thread->m_regs[dstReg] = MakeScriptValue(result);
    }

    HYP_FORCE_INLINE void Xor(
        BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Value* lhs = &thread->m_regs[lhsReg];
        Value* rhs = &thread->m_regs[rhsReg];
        
        const NumericType numericType = MATCH_TYPES(lhs->GetNumericType(), rhs->GetNumericType());

        Number a, b;
        Number result { numericType };

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            HYP_NUMERIC_OPERATION_BITWISE(a, b, ^);
        }
        else
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "XOR",
                    lhs->GetTypeString(),
                    rhs->GetTypeString()));
            return;
        }

        // set the destination register to be the result
        thread->m_regs[dstReg] = MakeScriptValue(result);
    }

    HYP_FORCE_INLINE void Shl(BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Value* lhs = &thread->m_regs[lhsReg];
        Value* rhs = &thread->m_regs[rhsReg];
        
        const NumericType numericType = MATCH_TYPES(lhs->GetNumericType(), rhs->GetNumericType());

        Number a, b;
        Number result { numericType };

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            HYP_NUMERIC_OPERATION_BITWISE(a, b, <<);
        }
        else
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "SHL",
                    lhs->GetTypeString(),
                    rhs->GetTypeString()));
            return;
        }

        // set the destination register to be the result
        thread->m_regs[dstReg] = MakeScriptValue(result);
    }

    HYP_FORCE_INLINE void Shr(BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Value* lhs = &thread->m_regs[lhsReg];
        Value* rhs = &thread->m_regs[rhsReg];
        
        const NumericType numericType = MATCH_TYPES(lhs->GetNumericType(), rhs->GetNumericType());

        Number a, b;
        Number result { numericType };

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            HYP_NUMERIC_OPERATION_BITWISE(a, b, >>);
        }
        else
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "SHR",
                    lhs->GetTypeString(),
                    rhs->GetTypeString()));
            return;
        }

        // set the destination register to be the result
        thread->m_regs[dstReg] = MakeScriptValue(result);
    }

    HYP_FORCE_INLINE void Not(BCRegister reg)
    {
        // load value from register
        Value& value = thread->m_regs[reg];

        Number num;

        Number result;
        result.flags = num.flags;
        
        // we only allow bitwise NOT on integers
        if (value.GetNumber(&num) && (num.flags & (Number::FLAG_SIGNED | Number::FLAG_UNSIGNED)))
        {
            if (num.flags & Number::FLAG_SIGNED)
            {
                if (num.flags & Number::FLAG_8_BIT)
                {
                    result.i = ~static_cast<int8>(num.i);
                }
                else if (num.flags & Number::FLAG_16_BIT)
                {
                    result.i = ~static_cast<int16>(num.i);
                }
                else if (num.flags & Number::FLAG_32_BIT)
                {
                    result.i = ~static_cast<int32>(num.i);
                }
                else
                {
                    result.i = ~num.i;
                }
            }
            else if (num.flags & Number::FLAG_UNSIGNED)
            {
                if (num.flags & Number::FLAG_8_BIT)
                {
                    result.u = ~static_cast<uint8>(num.u);
                }
                else if (num.flags & Number::FLAG_16_BIT)
                {
                    result.u = ~static_cast<uint16>(num.u);
                }
                else if (num.flags & Number::FLAG_32_BIT)
                {
                    result.u = ~static_cast<uint32>(num.u);
                }
                else
                {
                    result.u = ~num.u;
                }
            }
            else
            {
                HYP_UNREACHABLE();
            }
        }
        else
        {
            state->ThrowException(
                thread,
                Exception::InvalidBitwiseArgument());
            return;
        }

        value = MakeScriptValue(result);
    }

    HYP_FORCE_INLINE void Throw(BCRegister reg)
    {
        // load value from register
        Value* value = &thread->m_regs[reg];

        // @TODO Allow throwing the arugment

        state->ThrowException(
            thread,
            Exception("User exception"));
    }

    HYP_FORCE_INLINE void ExportSymbol(BCRegister reg, uint32_t hash)
    {
        if (!state->GetExportedSymbols().Store(hash, thread->m_regs[reg]).second)
        {
            state->ThrowException(
                thread,
                Exception::DuplicateExportException());
        }
    }

    HYP_FORCE_INLINE void Neg(BCRegister reg)
    {
        // load value from register
        Value& value = thread->m_regs[reg];

        Number num;

        if (!value.GetNumber(&num))
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "NEG",
                    value.GetTypeString()));

            return;
        }

        Number result;
        result.flags = num.flags;

        if (num.flags & Number::FLAG_SIGNED)
        {
            result.i = -num.i;
        }
        else if (num.flags & Number::FLAG_UNSIGNED)
        {
            result.u = static_cast<uint64>(-static_cast<int64>(num.u));
            result.flags = Number::FLAG_SIGNED | (num.flags & (Number::FLAG_8_BIT | Number::FLAG_16_BIT | Number::FLAG_32_BIT));
        }
        else
        {
            result.f = -num.f;
        }

        value = MakeScriptValue(result);
    }

    HYP_FORCE_INLINE void CastU8(BCRegister dst, BCRegister src)
    {
        // load value from register
        Value& value = thread->m_regs[src];

        Number num;

        if (!value.GetNumber(&num))
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_U8",
                    value.GetTypeString()));

            return;
        }

        Number result;
        result.flags = Number::FLAG_UNSIGNED | Number::FLAG_8_BIT;

        if (num.flags & Number::FLAG_UNSIGNED)
        {
            result.u = static_cast<uint8>(num.u);
        }
        else if (num.flags & Number::FLAG_SIGNED)
        {
            result.u = static_cast<uint8>(num.i);
        }
        else
        {
            result.u = static_cast<uint8>(num.f);
        }

        thread->m_regs[dst] = MakeScriptValue(result);
    }

    HYP_FORCE_INLINE void CastU16(BCRegister dst, BCRegister src)
    {
        // load value from register
        Value& value = thread->m_regs[src];

        Number num;

        if (!value.GetNumber(&num))
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_U16",
                    value.GetTypeString()));

            return;
        }

        Number result;
        result.flags = Number::FLAG_UNSIGNED | Number::FLAG_16_BIT;

        if (num.flags & Number::FLAG_UNSIGNED)
        {
            result.u = static_cast<uint16>(num.u);
        }
        else if (num.flags & Number::FLAG_SIGNED)
        {
            result.u = static_cast<uint16>(num.i);
        }
        else
        {
            result.u = static_cast<uint16>(num.f);
        }
        
        thread->m_regs[dst] = MakeScriptValue(result);
    }

    HYP_FORCE_INLINE void CastU32(BCRegister dst, BCRegister src)
    {
        // load value from register
        Value& value = thread->m_regs[src];
        Number num;

        if (!value.GetNumber(&num))
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_U32",
                    value.GetTypeString()));
            return;
        }

        Number result;
        result.flags = Number::FLAG_UNSIGNED | Number::FLAG_32_BIT;

        if (num.flags & Number::FLAG_UNSIGNED)
        {
            result.u = static_cast<uint32>(num.u);
        }
        else if (num.flags & Number::FLAG_SIGNED)
        {
            result.u = static_cast<uint32>(num.i);
        }
        else
        {
            result.u = static_cast<uint32>(num.f);
        }

        thread->m_regs[dst] = MakeScriptValue(result);
    }

    HYP_FORCE_INLINE void CastU64(BCRegister dst, BCRegister src)
    {
        // load value from register
        Value& value = thread->m_regs[src];
        Number num;

        if (!value.GetNumber(&num))
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_U64",
                    value.GetTypeString()));
            return;
        }

        Number result;
        result.flags = Number::FLAG_UNSIGNED;

        if (num.flags & Number::FLAG_UNSIGNED)
        {
            result.u = num.u;
        }
        else if (num.flags & Number::FLAG_SIGNED)
        {
            result.u = static_cast<uint64>(num.i);
        }
        else
        {
            result.u = static_cast<uint64>(num.f);
        }

        thread->m_regs[dst] = MakeScriptValue(result);
    }

    HYP_FORCE_INLINE void CastI8(BCRegister dst, BCRegister src)
    {
        Value& value = thread->m_regs[src];
        Number num;

        if (!value.GetNumber(&num))
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_I8",
                    value.GetTypeString()));
            return;
        }

        Number result;
        result.flags = Number::FLAG_SIGNED | Number::FLAG_8_BIT;

        if (num.flags & Number::FLAG_UNSIGNED)
        {
            result.i = static_cast<int8>(num.u);
        }
        else if (num.flags & Number::FLAG_SIGNED)
        {
            result.i = static_cast<int8>(num.i);
        }
        else
        {
            result.i = static_cast<int8>(num.f);
        }

        thread->m_regs[dst] = MakeScriptValue(result);
    }

    HYP_FORCE_INLINE void CastI16(BCRegister dst, BCRegister src)
    {
        Value& value = thread->m_regs[src];
        Number num;

        if (!value.GetNumber(&num))
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_I16",
                    value.GetTypeString()));
            return;
        }

        Number result;
        result.flags = Number::FLAG_SIGNED | Number::FLAG_16_BIT;

        if (num.flags & Number::FLAG_UNSIGNED)
        {
            result.i = static_cast<int16>(num.u);
        }
        else if (num.flags & Number::FLAG_SIGNED)
        {
            result.i = static_cast<int16>(num.i);
        }
        else
        {
            result.i = static_cast<int16>(num.f);
        }

        thread->m_regs[dst] = MakeScriptValue(result);
    }

    HYP_FORCE_INLINE void CastI32(BCRegister dst, BCRegister src)
    {
        Value& value = thread->m_regs[src];
        Number num;

        if (!value.GetNumber(&num))
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_I32",
                    value.GetTypeString()));
            return;
        }

        Number result;
        result.flags = Number::FLAG_SIGNED | Number::FLAG_32_BIT;

        if (num.flags & Number::FLAG_UNSIGNED)
        {
            result.i = static_cast<int32>(num.u);
        }
        else if (num.flags & Number::FLAG_SIGNED)
        {
            result.i = static_cast<int32>(num.i);
        }
        else
        {
            result.i = static_cast<int32>(num.f);
        }

        thread->m_regs[dst] = MakeScriptValue(result);
    }

    HYP_FORCE_INLINE void CastI64(BCRegister dst, BCRegister src)
    {
        Value& value = thread->m_regs[src];
        Number num;

        if (!value.GetNumber(&num))
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_I64",
                    value.GetTypeString()));
            return;
        }

        Number result;
        result.flags = Number::FLAG_SIGNED;

        if (num.flags & Number::FLAG_UNSIGNED)
        {
            result.i = static_cast<int64>(num.u);
        }
        else if (num.flags & Number::FLAG_SIGNED)
        {
            result.i = num.i;
        }
        else
        {
            result.i = static_cast<int64>(num.f);
        }

        thread->m_regs[dst] = MakeScriptValue(result);
    }

    HYP_FORCE_INLINE void CastF32(BCRegister dst, BCRegister src)
    {
        // load value from register
        Value& value = thread->m_regs[src];
        Number num;

        if (!value.GetNumber(&num))
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_F32",
                    value.GetTypeString()));
            return;
        }

        Number result;
        result.flags = Number::FLAG_FLOATING_POINT | Number::FLAG_32_BIT;

        if (num.flags & Number::FLAG_UNSIGNED)
        {
            result.f = static_cast<float>(num.u);
        }
        else if (num.flags & Number::FLAG_SIGNED)
        {
            result.f = static_cast<float>(num.i);
        }
        else
        {
            result.f = static_cast<float>(num.f);
        }

        thread->m_regs[dst] = MakeScriptValue(result);
    }

    HYP_FORCE_INLINE void CastF64(BCRegister dst, BCRegister src)
    {
        // load value from register
        Value& value = thread->m_regs[src];
        Number num;

        if (!value.GetNumber(&num))
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_F64",
                    value.GetTypeString()));
            return;
        }

        Number result;
        result.flags = Number::FLAG_FLOATING_POINT;

        if (num.flags & Number::FLAG_UNSIGNED)
        {
            result.f = static_cast<double>(num.u);
        }
        else if (num.flags & Number::FLAG_SIGNED)
        {
            result.f = static_cast<double>(num.i);
        }
        else
        {
            result.f = num.f;
        }

        thread->m_regs[dst] = MakeScriptValue(result);
    }

    HYP_FORCE_INLINE void CastBool(BCRegister dst, BCRegister src)
    {
        // load value from register
        Value& value = thread->m_regs[src];

        // use same logic as CmpZ to determine truthiness
        bool result = false;
        Number num;

        if (value.GetSignedOrUnsigned(&num))
        {
            result = (num.flags & Number::FLAG_SIGNED) ? (num.i != 0) : (num.u != 0);
        }
        else if (value.GetFloatingPoint(&num.f))
        {
            result = (num.f != 0.0);
        }
        else if (value.GetBoolean(&result))
        {
            // already a bool, do nothing
        }
        else
        {
            void* ptrValue = value.ToRef().GetPointer();
            result = (ptrValue != nullptr);
        }

        thread->m_regs[dst] = MakeScriptValue(result);
    }

    HYP_FORCE_INLINE void CastDynamic(BCRegister dst, BCRegister src) // come back to this
    {
        // load the VMObject from dst
        Value& value = thread->m_regs[dst];

        // Ensure it is a VMObject
        VMObject* classObjectPtr = value.GetObject();

        if (!classObjectPtr)
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_DYNAMIC",
                    value.GetTypeString()));

            return;
        }

        // load the target from src
        Value& target = thread->m_regs[src];

        // Ensure it is a VMObject
        VMObject* targetObjectPtr = target.GetObject();

        if (!targetObjectPtr)
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_DYNAMIC",
                    target.GetTypeString()));

            return;
        }

        bool isInstance = false;

        // Check if the target is an instance of the type
        Value* pBase = nullptr;

        if (const Value& targetClassValue = targetObjectPtr->GetClassPointer(); targetClassValue.IsValid())
        {
            constexpr uint32 maxDepth = 1024;
            uint32 depth = 0;

            VMObject* targetClassObject = targetClassValue.GetObject();

            while (targetClassObject != nullptr && depth < maxDepth)
            {
                isInstance = (*targetClassObject == *classObjectPtr);

                if (isInstance)
                {
                    break;
                }

                if (!(targetClassObject->LookupBasePointer(&pBase) && (targetClassObject = pBase->GetObject())))
                {
                    break;
                }

                depth++;
            }

            if (depth == maxDepth)
            {
                state->ThrowException(
                    thread,
                    Exception::InvalidOperationException(
                        "CAST_DYNAMIC",
                        "Max depth reached"));

                return;
            }
        }

        // If it is not an instance, throw an exception
        if (!isInstance)
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_DYNAMIC",
                    "Not an instance"));

            return;
        }

        Assert(pBase != nullptr);

        // Set the destination register to be the target
        thread->m_regs[dst].AssignValue(MakeScriptValueRef(pBase), false);
    }
};

HYP_FORCE_INLINE static void HandleInstruction(
    InstructionHandler& handler,
    BytecodeStream* bs,
    ubyte code)
{

    switch (code)
    {
    case STORE_STATIC_STRING:
    {
        // get string length
        uint32 len;
        bs->Read(&len);

        // read string based on length
        char* str = new char[len + 1];
        bs->Read(str, len);
        str[len] = '\0';

        handler.StoreStaticString(
            len,
            str);

        delete[] str;

        break;
    }
    case STORE_STATIC_ADDRESS:
    {
        BCAddress addr;
        bs->Read(&addr);

        handler.StoreStaticAddress(
            addr);

        break;
    }
    case STORE_STATIC_FUNCTION:
    {
        BCAddress addr;
        bs->Read(&addr);
        uint8 nargs;
        bs->Read(&nargs);
        uint8 flags;
        bs->Read(&flags);

        handler.StoreStaticFunction(
            addr,
            nargs,
            flags);

        break;
    }
    case STORE_STATIC_TYPE:
    {
        uint16 typeNameLen;
        bs->Read(&typeNameLen);

        char* typeName = new char[typeNameLen + 1];
        typeName[typeNameLen] = '\0';
        bs->Read(typeName, typeNameLen);

        uint16 size;
        bs->Read(&size);

        Assert(size > 0);

        char** names = new char*[size];
        // load each name
        for (int i = 0; i < size; i++)
        {
            uint16 length;
            bs->Read(&length);

            names[i] = new char[length + 1];
            names[i][length] = '\0';
            bs->Read(names[i], length);
        }

        handler.StoreStaticType(
            typeName,
            size,
            names);

        delete[] typeName;

        // delete the names
        for (uint16 i = 0; i < size; i++)
        {
            delete[] names[i];
        }

        delete[] names;

        break;
    }
    case LOAD_I32:
    {
        BCRegister reg;
        bs->Read(&reg);
        int32_t i32;
        bs->Read(&i32);

        handler.LoadI32(
            reg,
            i32);

        break;
    }
    case LOAD_I64:
    {
        BCRegister reg;
        bs->Read(&reg);
        int64_t i64;
        bs->Read(&i64);

        handler.LoadI64(
            reg,
            i64);

        break;
    }
    case LOAD_U32:
    {
        BCRegister reg;
        bs->Read(&reg);
        uint32 u32;
        bs->Read(&u32);

        handler.LoadU32(
            reg,
            u32);

        break;
    }
    case LOAD_U64:
    {
        BCRegister reg;
        bs->Read(&reg);
        uint64_t u64;
        bs->Read(&u64);

        handler.LoadU64(
            reg,
            u64);

        break;
    }
    case LOAD_F32:
    {
        BCRegister reg;
        bs->Read(&reg);
        float f32;
        bs->Read(&f32);

        handler.LoadF32(
            reg,
            f32);

        break;
    }
    case LOAD_F64:
    {
        BCRegister reg;
        bs->Read(&reg);
        double f64;
        bs->Read(&f64);

        handler.LoadF64(
            reg,
            f64);

        break;
    }
    case LOAD_OFFSET:
    {
        BCRegister reg;
        bs->Read(&reg);
        uint16 offset;
        bs->Read(&offset);

        handler.LoadOffset(
            reg,
            offset);

        break;
    }
    case LOAD_INDEX:
    {
        BCRegister reg;
        bs->Read(&reg);
        uint16 index;
        bs->Read(&index);

        handler.LoadIndex(
            reg,
            index);

        break;
    }
    case LOAD_STATIC:
    {
        BCRegister reg;
        bs->Read(&reg);
        uint16 index;
        bs->Read(&index);

        handler.LoadStatic(
            reg,
            index);

        break;
    }
    case LOAD_STRING:
    {
        BCRegister reg;
        bs->Read(&reg);
        // get string length
        uint32 len;
        bs->Read(&len);

        // read string based on length
        char* str = new char[len + 1];
        str[len] = '\0';
        bs->Read(str, len);

        handler.LoadConstantString(
            reg,
            len,
            str);

        delete[] str;

        break;
    }
    case LOAD_ADDR:
    {
        BCRegister reg;
        bs->Read(&reg);
        BCAddress addr;
        bs->Read(&addr);

        handler.LoadAddr(
            reg,
            addr);

        break;
    }
    case LOAD_FUNC:
    {
        BCRegister reg;
        bs->Read(&reg);
        BCAddress addr;
        bs->Read(&addr);
        uint8 nargs;
        bs->Read(&nargs);
        uint8 flags;
        bs->Read(&flags);

        handler.LoadFunc(
            reg,
            addr,
            nargs,
            flags);

        break;
    }
    case LOAD_TYPE:
    {
        BCRegister reg;
        bs->Read(&reg);
        uint16 typeNameLen;
        bs->Read(&typeNameLen);

        char* typeName = new char[typeNameLen + 1];
        typeName[typeNameLen] = '\0';
        bs->Read(typeName, typeNameLen);

        // number of members
        uint16 size;
        bs->Read(&size);

        char** names = new char*[size];

        // load each name
        for (uint16 i = 0; i < size; i++)
        {
            uint16 length;
            bs->Read(&length);

            names[i] = new char[length + 1];
            names[i][length] = '\0';
            bs->Read(names[i], length);
        }

        handler.LoadType(
            reg,
            typeNameLen,
            typeName,
            size,
            names);

        delete[] typeName;

        // delete the names
        for (uint16 i = 0; i < size; i++)
        {
            delete[] names[i];
        }

        delete[] names;

        break;
    }
    case LOAD_MEM:
    {
        BCRegister dst;
        bs->Read(&dst);
        BCRegister src;
        bs->Read(&src);
        uint8 index;
        bs->Read(&index);

        handler.LoadMem(
            dst,
            src,
            index);

        break;
    }
    case LOAD_MEM_HASH:
    {
        BCRegister dst;
        bs->Read(&dst);
        BCRegister src;
        bs->Read(&src);
        uint32 hash;
        bs->Read(&hash);

        handler.LoadMemHash(
            dst,
            src,
            hash);

        break;
    }
    case LOAD_ARRAYIDX:
    {
        BCRegister dstReg;
        bs->Read(&dstReg);
        BCRegister srcReg;
        bs->Read(&srcReg);
        BCRegister indexReg;
        bs->Read(&indexReg);

        handler.LoadArrayIdx(
            dstReg,
            srcReg,
            indexReg);

        break;
    }
    case LOAD_OFFSET_REF:
    {
        BCRegister reg;
        bs->Read(&reg);
        uint16 offset;
        bs->Read(&offset);

        handler.LoadOffsetRef(
            reg,
            offset);

        break;
    }
    case LOAD_INDEX_REF:
    {
        BCRegister reg;
        bs->Read(&reg);
        uint16 index;
        bs->Read(&index);

        handler.LoadIndexRef(
            reg,
            index);

        break;
    }
    case REF:
    {
        BCRegister dstReg;
        BCRegister srcReg;

        bs->Read(&dstReg);
        bs->Read(&srcReg);

        handler.LoadRef(
            dstReg,
            srcReg);

        break;
    }
    case DEREF:
    {
        BCRegister dstReg;
        BCRegister srcReg;

        bs->Read(&dstReg);
        bs->Read(&srcReg);

        handler.LoadDeref(
            dstReg,
            srcReg);

        break;
    }
    case LOAD_NULL:
    {
        BCRegister reg;
        bs->Read(&reg);

        handler.LoadNull(
            reg);

        break;
    }
    case LOAD_TRUE:
    {
        BCRegister reg;
        bs->Read(&reg);

        handler.LoadTrue(
            reg);

        break;
    }
    case LOAD_FALSE:
    {
        BCRegister reg;
        bs->Read(&reg);

        handler.LoadFalse(
            reg);

        break;
    }
    case MOV_OFFSET:
    {
        uint16 offset;
        bs->Read(&offset);
        BCRegister reg;
        bs->Read(&reg);

        handler.MovOffset(
            offset,
            reg);

        break;
    }
    case MOV_INDEX:
    {
        uint16 index;
        bs->Read(&index);
        BCRegister reg;
        bs->Read(&reg);

        handler.MovIndex(
            index,
            reg);

        break;
    }
    case MOV_STATIC:
    {
        uint16 index;
        bs->Read(&index);
        BCRegister reg;
        bs->Read(&reg);

        handler.MovStatic(
            index,
            reg);

        break;
    }
    case MOV_MEM:
    {
        BCRegister dst;
        bs->Read(&dst);
        uint8 index;
        bs->Read(&index);
        BCRegister src;
        bs->Read(&src);

        handler.MovMem(
            dst,
            index,
            src);

        break;
    }
    case MOV_MEM_HASH:
    {
        BCRegister dst;
        bs->Read(&dst);
        uint32 hash;
        bs->Read(&hash);
        BCRegister src;
        bs->Read(&src);

        handler.MovMemHash(
            dst,
            hash,
            src);

        break;
    }
    case MOV_ARRAYIDX:
    {
        BCRegister dst;
        bs->Read(&dst);
        uint32 index;
        bs->Read(&index); // should be int64?
        BCRegister src;
        bs->Read(&src);

        handler.MovArrayIdx(
            dst,
            index,
            src);

        break;
    }
    case MOV_ARRAYIDX_REG:
    {
        BCRegister dst;
        bs->Read(&dst);
        BCRegister indexReg;
        bs->Read(&indexReg);
        BCRegister src;
        bs->Read(&src);

        handler.MovArrayIdxReg(
            dst,
            indexReg,
            src);

        break;
    }
    case MOV_REG:
    {
        BCRegister dst;
        bs->Read(&dst);
        BCRegister src;
        bs->Read(&src);

        handler.MovReg(
            dst,
            src);

        break;
    }
    case HAS_MEM_HASH:
    {
        BCRegister dst;
        bs->Read(&dst);
        BCRegister src;
        bs->Read(&src);
        uint32 hash;
        bs->Read(&hash);

        handler.HasMemHash(
            dst,
            src,
            hash);

        break;
    }
    case PUSH:
    {
        BCRegister reg;
        bs->Read(&reg);

        handler.Push(
            reg);

        break;
    }
    case POP:
    {
        handler.Pop();

        break;
    }
    case PUSH_ARRAY:
    {
        BCRegister dst;
        bs->Read(&dst);
        BCRegister src;
        bs->Read(&src);

        handler.PushArray(
            dst,
            src);

        break;
    }
    case ADD_SP:
    {
        uint16 val;
        bs->Read(&val);

        handler.AddSp(
            val);

        break;
    }
    case SUB_SP:
    {
        uint16 val;
        bs->Read(&val);

        handler.SubSp(
            val);

        break;
    }
    case JMP:
    {
        // BCRegister reg; bs->Read(&reg);
        BCAddress addr;
        bs->Read(&addr);

        handler.Jmp(
            addr);

        break;
    }
    case JE:
    {
        BCAddress addr;
        bs->Read(&addr);

        handler.Je(
            addr);

        break;
    }
    case JNE:
    {
        BCAddress addr;
        bs->Read(&addr);

        handler.Jne(
            addr);

        break;
    }
    case JG:
    {
        BCAddress addr;
        bs->Read(&addr);

        handler.Jg(
            addr);

        break;
    }
    case JGE:
    {
        BCAddress addr;
        bs->Read(&addr);

        handler.Jge(
            addr);

        break;
    }
    case CALL:
    {
        BCRegister reg;
        bs->Read(&reg);
        uint8 nargs;
        bs->Read(&nargs);

        handler.Call(
            reg,
            nargs);

        break;
    }
    case RET:
    {
        handler.Ret();

        break;
    }
    case BEGIN_TRY:
    {
        BCAddress catchAddress;
        bs->Read(&catchAddress);

        handler.BeginTry(
            catchAddress);

        break;
    }
    case END_TRY:
    {
        handler.EndTry();

        break;
    }
    case NEW:
    {
        BCRegister dst;
        bs->Read(&dst);
        BCRegister src;
        bs->Read(&src);

        handler.New(
            dst,
            src);

        break;
    }
    case NEW_ARRAY:
    {
        BCRegister dst;
        bs->Read(&dst);
        uint32 size;
        bs->Read(&size);

        handler.NewArray(
            dst,
            size);

        break;
    }
    case CMP:
    {
        BCRegister lhsReg;
        bs->Read(&lhsReg);
        BCRegister rhsReg;
        bs->Read(&rhsReg);

        handler.Cmp(
            lhsReg,
            rhsReg);

        break;
    }
    case CMPZ:
    {
        BCRegister reg;
        bs->Read(&reg);

        handler.CmpZ(
            reg);

        break;
    }
    case ADD:
    {
        BCRegister lhsReg;
        bs->Read(&lhsReg);
        BCRegister rhsReg;
        bs->Read(&rhsReg);
        BCRegister dstReg;
        bs->Read(&dstReg);

        handler.Add(
            lhsReg,
            rhsReg,
            dstReg);

        break;
    }
    case SUB:
    {
        BCRegister lhsReg;
        bs->Read(&lhsReg);
        BCRegister rhsReg;
        bs->Read(&rhsReg);
        BCRegister dstReg;
        bs->Read(&dstReg);

        handler.Sub(
            lhsReg,
            rhsReg,
            dstReg);

        break;
    }
    case MUL:
    {
        BCRegister lhsReg;
        bs->Read(&lhsReg);
        BCRegister rhsReg;
        bs->Read(&rhsReg);
        BCRegister dstReg;
        bs->Read(&dstReg);

        handler.Mul(
            lhsReg,
            rhsReg,
            dstReg);

        break;
    }
    case DIV:
    {
        BCRegister lhsReg;
        bs->Read(&lhsReg);
        BCRegister rhsReg;
        bs->Read(&rhsReg);
        BCRegister dstReg;
        bs->Read(&dstReg);

        handler.Div(
            lhsReg,
            rhsReg,
            dstReg);

        break;
    }
    case MOD:
    {
        BCRegister lhsReg;
        bs->Read(&lhsReg);
        BCRegister rhsReg;
        bs->Read(&rhsReg);
        BCRegister dstReg;
        bs->Read(&dstReg);

        handler.Mod(
            lhsReg,
            rhsReg,
            dstReg);

        break;
    }
    case AND:
    {
        BCRegister lhsReg;
        bs->Read(&lhsReg);
        BCRegister rhsReg;
        bs->Read(&rhsReg);
        BCRegister dstReg;
        bs->Read(&dstReg);

        handler.And(
            lhsReg,
            rhsReg,
            dstReg);

        break;
    }
    case OR:
    {
        BCRegister lhsReg;
        bs->Read(&lhsReg);
        BCRegister rhsReg;
        bs->Read(&rhsReg);
        BCRegister dstReg;
        bs->Read(&dstReg);

        handler.Or(
            lhsReg,
            rhsReg,
            dstReg);

        break;
    }
    case XOR:
    {
        BCRegister lhsReg;
        bs->Read(&lhsReg);
        BCRegister rhsReg;
        bs->Read(&rhsReg);
        BCRegister dstReg;
        bs->Read(&dstReg);

        handler.Xor(
            lhsReg,
            rhsReg,
            dstReg);

        break;
    }
    case SHL:
    {
        BCRegister lhsReg;
        bs->Read(&lhsReg);
        BCRegister rhsReg;
        bs->Read(&rhsReg);
        BCRegister dstReg;
        bs->Read(&dstReg);

        handler.Shl(
            lhsReg,
            rhsReg,
            dstReg);

        break;
    }
    case SHR:
    {
        BCRegister lhsReg;
        bs->Read(&lhsReg);
        BCRegister rhsReg;
        bs->Read(&rhsReg);
        BCRegister dstReg;
        bs->Read(&dstReg);

        handler.Shr(
            lhsReg,
            rhsReg,
            dstReg);

        break;
    }
    case NEG:
    {
        BCRegister reg;
        bs->Read(&reg);

        handler.Neg(
            reg);

        break;
    }
    case NOT:
    {
        BCRegister reg;
        bs->Read(&reg);

        handler.Not(
            reg);

        break;
    }
    case THROW:
    {
        BCRegister reg;
        bs->Read(&reg);

        handler.Throw(
            reg);

        break;
    }
    case TRACEMAP:
    {
        uint32 len;
        bs->Read(&len);

        uint32 stringmapCount;
        bs->Read(&stringmapCount);

        Tracemap::StringmapEntry* stringmap = nullptr;

        if (stringmapCount != 0)
        {
            stringmap = new Tracemap::StringmapEntry[stringmapCount];

            for (uint32 i = 0; i < stringmapCount; i++)
            {
                bs->Read(&stringmap[i].entryType);
                bs->ReadZeroTerminatedString(stringmap[i].data);
            }
        }

        uint32 linemapCount;
        bs->Read(&linemapCount);

        Tracemap::LinemapEntry* linemap = nullptr;

        if (linemapCount != 0)
        {
            linemap = new Tracemap::LinemapEntry[linemapCount];
            bs->Read(linemap, sizeof(Tracemap::LinemapEntry) * linemapCount);
        }

        handler.state->m_tracemap.Set(stringmap, linemap);

        break;
    }
    case REM:
    {
        uint32 len;
        bs->Read(&len);
        // just skip comment
        bs->Skip(len);

        break;
    }
    case EXPORT:
    {
        BCRegister reg;
        bs->Read(&reg);
        uint32 hash;
        bs->Read(&hash);

        handler.ExportSymbol(reg, hash);

        break;
    }
    case CAST_U8:
    {
        BCRegister dst;
        bs->Read(&dst);
        BCRegister src;
        bs->Read(&src);

        handler.CastU8(
            dst,
            src);

        break;
    }
    case CAST_U16:
    {
        BCRegister dst;
        bs->Read(&dst);
        BCRegister src;
        bs->Read(&src);

        handler.CastU16(
            dst,
            src);

        break;
    }
    case CAST_U32:
    {
        BCRegister dst;
        bs->Read(&dst);
        BCRegister src;
        bs->Read(&src);

        handler.CastU32(
            dst,
            src);

        break;
    }
    case CAST_U64:
    {
        BCRegister dst;
        bs->Read(&dst);
        BCRegister src;
        bs->Read(&src);

        handler.CastU64(
            dst,
            src);

        break;
    }
    case CAST_I8:
    {
        BCRegister dst;
        bs->Read(&dst);
        BCRegister src;
        bs->Read(&src);

        handler.CastI8(
            dst,
            src);

        break;
    }
    case CAST_I16:
    {
        BCRegister dst;
        bs->Read(&dst);
        BCRegister src;
        bs->Read(&src);

        handler.CastI16(
            dst,
            src);

        break;
    }
    case CAST_I32:
    {
        BCRegister dst;
        bs->Read(&dst);
        BCRegister src;
        bs->Read(&src);

        handler.CastI32(
            dst,
            src);

        break;
    }
    case CAST_I64:
    {
        BCRegister dst;
        bs->Read(&dst);
        BCRegister src;
        bs->Read(&src);

        handler.CastI64(
            dst,
            src);

        break;
    }
    case CAST_F32:
    {
        BCRegister dst;
        bs->Read(&dst);
        BCRegister src;
        bs->Read(&src);

        handler.CastF32(
            dst,
            src);

        break;
    }
    case CAST_F64:
    {
        BCRegister dst;
        bs->Read(&dst);
        BCRegister src;
        bs->Read(&src);

        handler.CastF64(
            dst,
            src);

        break;
    }
    case CAST_BOOL:
    {
        BCRegister dst;
        bs->Read(&dst);
        BCRegister src;
        bs->Read(&src);

        handler.CastBool(
            dst,
            src);

        break;
    }
    case CAST_DYNAMIC:
    {
        BCRegister dst;
        bs->Read(&dst);
        BCRegister src;
        bs->Read(&src);

        handler.CastDynamic(
            dst,
            src);

        break;
    }
    default:
    {
        int64 lastPos = int64(bs->Position()) - sizeof(ubyte);
        HYP_FAIL("unknown instruction '{}' referenced at location {}", code, lastPos);
        // seek to end of bytecode stream
        bs->Seek(bs->Size());

        return;
    }
    }
}

VM::VM(APIInstance& apiInstance)
    : m_apiInstance(apiInstance)
{
    m_state.m_vm = this;
    // create main thread
    m_state.CreateThread();
}

VM::~VM()
{
}

void VM::PushNativeFunctionPtr(Script_NativeFunction ptr)
{
    Script_VMData vmData;
    vmData.type = Script_VMData::NATIVE_FUNCTION;
    vmData.nativeFunc = ptr;

    m_state.GetMainThread()->m_stack.Push(MakeScriptValue(vmData));
}

void VM::Invoke(InstructionHandler* handler, Value&& value, uint8 nargs)
{
    static const uint32 invokeHash = hashFnv1("$invoke");

    VMState* state = handler->state;
    Script_ExecutionThread* thread = handler->thread;
    BytecodeStream* bs = handler->bs;

    Assert(state != nullptr);
    Assert(thread != nullptr);
    Assert(bs != nullptr);

    if (value.IsFunction())
    {
        if (value.IsNativeFunction())
        {
            Value** args = new Value*[nargs > 0 ? nargs : 1];

            int64 i = static_cast<int64>(thread->m_stack.GetStackPointer()) - 1;
            for (int j = nargs - 1; j >= 0 && i >= 0; i--, j--)
            {
                args[j] = &thread->m_stack[i];
            }

            sdk::Params params {
                .apiInstance = m_apiInstance,
                .handler = handler,
                .args = args,
                .nargs = nargs
            };

            // disable auto gc so no collections happen during a native function
            state->enableAutoGc = false;

            // call the native function
            Script_VMData* vmData = value.GetVMData();
            Assert(vmData != nullptr && vmData->nativeFunc != nullptr);

            vmData->nativeFunc(params);

            // re-enable auto gc
            state->enableAutoGc = ENABLE_GC;

            delete[] args;

            return;
        }
        else if (VMObject* object = value.GetObject()) // functor object
        {
            if (Member* member = object->LookupMemberFromHash(invokeHash))
            {
                const int64 sp = static_cast<int64>(thread->m_stack.GetStackPointer());
                const int64 argsStart = sp - nargs;

                if (nargs > 0)
                {
                    // shift over by 1 -- and insert 'self' to start of args
                    // make a copy of last item to not overwrite it
                    thread->m_stack.Push(std::move(thread->m_stack[sp - 1]));

                    for (SizeType i = argsStart; i < sp - 1; i++)
                    {
                        thread->m_stack[i + 1].AssignValue(std::move(thread->m_stack[i]), false);
                    }

                    // set 'self' object to start of args
                    thread->m_stack[argsStart].AssignValue(std::move(value), false);
                }
                else
                {
                    thread->m_stack.Push(std::move(value));
                }

                Invoke(
                    handler,
                    MakeScriptValueRef(&member->value),
                    nargs + 1);

                Value& top = thread->m_stack.Top();

                Script_VMData* topVmData = top.GetVMData();
                Assert(topVmData != nullptr && topVmData->type == Script_VMData::FUNCTION_CALL);

                // bookkeeping to remove the closure object
                // normally, arguments are popped after the call is returned,
                // rather than within the body
                topVmData->call.varargsPush--;

                return;
            }
        }

        char buffer[255];
        std::sprintf(
            buffer,
            "cannot invoke type '%s' as a function",
            value.GetTypeString());

        state->ThrowException(
            thread,
            Exception(buffer));

        return;
    }

    // non-native function here
    Script_VMData* vmData = value.GetVMData();
    Assert(vmData != nullptr && vmData->type == Script_VMData::FUNCTION);

    if ((vmData->func.m_flags & FunctionFlags::VARIADIC) && nargs < vmData->func.m_nargs - 1)
    {
        // if variadic, make sure the arg count is /at least/ what is required
        state->ThrowException(
            thread,
            Exception::InvalidArgsException(
                vmData->func.m_nargs,
                nargs,
                true));
    }
    else if (!(vmData->func.m_flags & FunctionFlags::VARIADIC) && vmData->func.m_nargs != nargs)
    {
        state->ThrowException(
            thread,
            Exception::InvalidArgsException(
                vmData->func.m_nargs,
                nargs));
    }
    else
    {
        Script_VMData previousAddr;
        previousAddr.type = Script_VMData::FUNCTION_CALL;
        previousAddr.call.varargsPush = 0;
        previousAddr.call.returnAddress = static_cast<BCAddress>(bs->Position());

        if (vmData->func.m_flags & FunctionFlags::VARIADIC)
        {
            // for each argument that is over the expected size, we must pop it from
            // the stack and add it to a new array.
            int varargsAmt = nargs - vmData->func.m_nargs + 1;
            if (varargsAmt < 0)
            {
                varargsAmt = 0;
            }

            // set varargsPush value so we know how to get back to the stack size before.
            previousAddr.call.varargsPush = varargsAmt - 1;

            // allocate heap object
            HeapValue* hv = state->HeapAlloc(thread);
            Assert(hv != nullptr);

            // create VMArray object to hold variadic args
            VMArray arr(varargsAmt);

            for (int i = varargsAmt - 1; i >= 0; i--)
            {
                // push to array
                arr.AtIndex(i, std::move(thread->GetStack().Top()));
                thread->GetStack().Pop();
            }

            // push the array to the stack
            thread->GetStack().Push(MakeScriptValue(std::move(arr)));
        }

        // push the address
        thread->GetStack().Push(MakeScriptValue(previousAddr));

        // seek to the new address
        bs->Seek(vmData->func.m_addr);

        // increase function depth
        thread->m_funcDepth++;
    }
}

void VM::InvokeNow(
    BytecodeStream* bs,
    Value&& value,
    uint8 nargs)
{
    Script_ExecutionThread* thread = m_state.MAIN_THREAD;

    const SizeType positionBefore = bs->Position();
    const uint32 originalFunctionDepth = thread->m_funcDepth;
    const SizeType stackSizeBefore = thread->GetStack().GetStackPointer();

    InstructionHandler handler(
        &m_state,
        thread,
        bs);

    Script_VMData* pVmData = value.GetVMData();
    Assert(pVmData != nullptr);
    Assert(pVmData->type == Script_VMData::FUNCTION || pVmData->type == Script_VMData::NATIVE_FUNCTION);

    Script_VMData vmData = *pVmData;

    Invoke(&handler, std::move(value), nargs);

    if (vmData.type == Script_VMData::FUNCTION)
    { // don't do this for native function calls
        ubyte code;

        while (!bs->Eof())
        {
            bs->Read(&code);

            HandleInstruction(
                handler,
                bs,
                code);

            if (handler.thread->GetExceptionState().HasExceptionOccurred())
            {
                if (!HandleException(&handler))
                {
                    thread->m_exceptionState.m_exceptionDepth = 0;

                    Assert(thread->GetStack().GetStackPointer() >= stackSizeBefore);
                    thread->GetStack().Pop(thread->GetStack().GetStackPointer() - stackSizeBefore);

                    break;
                }
            }

            if (code == RET)
            {
                if (thread->m_funcDepth == originalFunctionDepth)
                {
                    break;
                }
            }
        }
    }

    bs->SetPosition(positionBefore);
}

void VM::CreateStackTrace(Script_ExecutionThread* thread, StackTrace* out)
{
    const SizeType maxStackTraceSize = std::size(out->callAddresses);

    for (int& callAddress : out->callAddresses)
    {
        callAddress = -1;
    }

    SizeType numRecordedCallAddresses = 0;

    for (SizeType sp = thread->m_stack.GetStackPointer(); sp != 0; sp--)
    {
        if (numRecordedCallAddresses >= maxStackTraceSize)
        {
            break;
        }

        const Value& top = thread->m_stack[sp - 1];

        Script_VMData* topVmData = top.GetVMData();
        Assert(topVmData != nullptr);

        if (topVmData->type == Script_VMData::FUNCTION_CALL)
        {
            out->callAddresses[numRecordedCallAddresses++] = static_cast<int>(topVmData->call.returnAddress);
        }
    }
}

bool VM::HandleException(InstructionHandler* handler)
{
    Script_ExecutionThread* thread = handler->thread;
    BytecodeStream* bs = handler->bs;

    if (thread->m_exceptionState.m_tryCounter != 0)
    {
        // handle exception
        --thread->m_exceptionState.m_tryCounter;

        Assert(thread->m_exceptionState.m_exceptionDepth != 0);
        --thread->m_exceptionState.m_exceptionDepth;

        Value* top = &thread->m_stack.Top();
        Script_VMData* topVmData = top->GetVMData();

        while (topVmData && topVmData->type != Script_VMData::TRY_CATCH_INFO)
        {
            thread->m_stack.Pop();

            top = &thread->m_stack.Top();
            topVmData = top->GetVMData();
        }

        // top should be exception data
        Assert(topVmData && topVmData->type != Script_VMData::TRY_CATCH_INFO);

        // jump to the catch block
        bs->Seek(topVmData->tryCatchInfo.catchAddress);

        // pop exception data from stack
        thread->m_stack.Pop();

        return true;
    }
    else
    {
        StackTrace stackTrace;
        CreateStackTrace(thread, &stackTrace);

        std::cout << "stackTrace = \n";

        for (auto callAddress : stackTrace.callAddresses)
        {
            if (callAddress == -1)
            {
                break;
            }

            std::cout << "\t" << std::hex << callAddress << "\n";
        }

        std::cout << "=====\n";

        // TODO: Seek outside function, if calling from outside?
        // so we can keep calling
    }

    return false;
}

void VM::Execute(BytecodeStream* bs)
{
    Assert(bs != nullptr);
    Assert(m_state.GetNumThreads() != 0);

    InstructionHandler handler(
        &m_state,
        m_state.MAIN_THREAD,
        bs);

    ubyte code;

    while (!bs->Eof())
    {
        bs->Read(&code);

        HandleInstruction(
            handler,
            bs,
            code);

        if (handler.thread->GetExceptionState().HasExceptionOccurred())
        {
            HandleException(&handler);

            if (!handler.state->good)
            {
                DebugLog(LogType::Error, "Unhandled exception in VM, stopping execution...\n");

                break;
            }
        }
    }
}

} // namespace vm
} // namespace hyperion
