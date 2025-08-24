#pragma once

#include <script/vm/BytecodeStream.hpp>
#include <script/vm/VM.hpp>
#include <script/vm/Value.hpp>
#include <script/vm/HeapValue.hpp>
#include <script/vm/VMArray.hpp>
#include <script/vm/VMMemoryBuffer.hpp>
#include <script/vm/VMObject.hpp>
#include <script/vm/VMString.hpp>
#include <script/vm/VMTypeInfo.hpp>

#include <script/Instructions.hpp>
#include <script/Hasher.hpp>

#include <core/Defines.hpp>
#include <core/debug/Debug.hpp>
#include <core/Types.hpp>

#define HYP_NUMERIC_OPERATION(a, b, oper)                                              \
    do                                                                                 \
    {                                                                                  \
        switch (result.m_type)                                                         \
        {                                                                              \
        case Value::I8:                                                                \
            result.m_value.i8 = static_cast<int8>(a.i) oper static_cast<int8>(b.i);    \
            break;                                                                     \
        case Value::I16:                                                               \
            result.m_value.i16 = static_cast<int16>(a.i) oper static_cast<int16>(b.i); \
            break;                                                                     \
        case Value::I32:                                                               \
            result.m_value.i32 = static_cast<int32>(a.i) oper static_cast<int32>(b.i); \
            break;                                                                     \
        case Value::I64:                                                               \
            result.m_value.i64 = a.i oper b.i;                                         \
            break;                                                                     \
        case Value::U8:                                                                \
            if (a.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.m_value.u8 = static_cast<uint8>(a.i);                           \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.m_value.u8 = static_cast<uint8>(a.u);                           \
            }                                                                          \
            if (b.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.m_value.u8 oper## = static_cast<uint8>(b.i);                    \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.m_value.u8 oper## = static_cast<uint8>(b.u);                    \
            }                                                                          \
            break;                                                                     \
        case Value::U16:                                                               \
            if (a.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.m_value.u16 = static_cast<uint16>(a.i);                         \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.m_value.u16 = static_cast<uint16>(a.u);                         \
            }                                                                          \
            if (b.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.m_value.u16 oper## = static_cast<uint16>(b.i);                  \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.m_value.u16 oper## = static_cast<uint16>(b.u);                  \
            }                                                                          \
            break;                                                                     \
        case Value::U32:                                                               \
            if (a.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.m_value.u32 = static_cast<uint32>(a.i);                         \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.m_value.u32 = static_cast<uint32>(a.u);                         \
            }                                                                          \
            if (b.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.m_value.u32 oper## = static_cast<uint32>(b.i);                  \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.m_value.u32 oper## = static_cast<uint32>(b.u);                  \
            }                                                                          \
            break;                                                                     \
        case Value::U64:                                                               \
            if (a.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.m_value.u64 = static_cast<uint64>(a.i);                         \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.m_value.u64 = a.u;                                              \
            }                                                                          \
            if (b.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.m_value.u64 oper## = static_cast<uint64>(b.i);                  \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.m_value.u64 oper## = b.u;                                       \
            }                                                                          \
            break;                                                                     \
        case Value::F32:                                                               \
            if (a.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.m_value.f = static_cast<float>(a.i);                            \
            }                                                                          \
            else if (a.flags & Number::FLAG_UNSIGNED)                                  \
            {                                                                          \
                result.m_value.f = static_cast<float>(a.u);                            \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.m_value.f = static_cast<float>(a.f);                            \
            }                                                                          \
            if (b.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.m_value.f oper## = static_cast<float>(b.i);                     \
            }                                                                          \
            else if (a.flags & Number::FLAG_UNSIGNED)                                  \
            {                                                                          \
                result.m_value.f oper## = static_cast<float>(b.u);                     \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.m_value.f oper## = static_cast<float>(b.f);                     \
            }                                                                          \
            break;                                                                     \
        case Value::F64:                                                               \
            if (a.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.m_value.d = static_cast<double>(a.i);                           \
            }                                                                          \
            else if (a.flags & Number::FLAG_UNSIGNED)                                  \
            {                                                                          \
                result.m_value.d = static_cast<double>(a.u);                           \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.m_value.d = a.f;                                                \
            }                                                                          \
            if (b.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.m_value.d oper## = static_cast<double>(b.i);                    \
            }                                                                          \
            else if (a.flags & Number::FLAG_UNSIGNED)                                  \
            {                                                                          \
                result.m_value.d oper## = static_cast<double>(b.u);                    \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.m_value.d oper## = b.f;                                         \
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
        switch (result.m_type)                                                         \
        {                                                                              \
        case Value::I8:                                                                \
            result.m_value.i8 = static_cast<int8>(a.i) oper static_cast<int8>(b.i);    \
            break;                                                                     \
        case Value::I16:                                                               \
            result.m_value.i16 = static_cast<int16>(a.i) oper static_cast<int16>(b.i); \
            break;                                                                     \
        case Value::I32:                                                               \
            result.m_value.i32 = static_cast<int32>(a.i) oper static_cast<int32>(b.i); \
            break;                                                                     \
        case Value::I64:                                                               \
            result.m_value.i64 = a.i oper b.i;                                         \
            break;                                                                     \
        case Value::U8:                                                                \
            if (a.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.m_value.u8 = static_cast<uint8>(a.i);                           \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.m_value.u8 = static_cast<uint8>(a.u);                           \
            }                                                                          \
            if (b.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.m_value.u8 oper## = static_cast<uint8>(b.i);                    \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.m_value.u8 oper## = static_cast<uint8>(b.u);                    \
            }                                                                          \
            break;                                                                     \
        case Value::U16:                                                               \
            if (a.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.m_value.u16 = static_cast<uint16>(a.i);                         \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.m_value.u16 = static_cast<uint16>(a.u);                         \
            }                                                                          \
            if (b.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.m_value.u16 oper## = static_cast<uint16>(b.i);                  \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.m_value.u16 oper## = static_cast<uint16>(b.u);                  \
            }                                                                          \
            break;                                                                     \
        case Value::U32:                                                               \
            if (a.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.m_value.u32 = static_cast<uint32>(a.i);                         \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.m_value.u32 = static_cast<uint32>(a.u);                         \
            }                                                                          \
            if (b.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.m_value.u32 oper## = static_cast<uint32>(b.i);                  \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.m_value.u32 oper## = static_cast<uint32>(b.u);                  \
            }                                                                          \
            break;                                                                     \
        case Value::U64:                                                               \
            if (a.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.m_value.u64 = static_cast<uint64>(a.i);                         \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.m_value.u64 = a.u;                                              \
            }                                                                          \
            if (b.flags & Number::FLAG_SIGNED)                                         \
            {                                                                          \
                result.m_value.u64 oper## = static_cast<uint64>(b.i);                  \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                result.m_value.u64 oper## = b.u;                                       \
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

class InstructionHandler
{
public:
    VMState* state;
    ExecutionThread* thread;
    BytecodeStream* bs;

    InstructionHandler(
        VMState* state,
        ExecutionThread* thread,
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
        // get register value given
        Value value;
        value.m_type = Value::I32;
        value.m_value.i32 = i32;

        thread->m_regs[reg].AssignValue(value, false);
    }

    HYP_FORCE_INLINE void LoadI64(BCRegister reg, int64 i64)
    {
        // get register value given
        Value value;
        value.m_type = Value::I64;
        value.m_value.i64 = i64;

        thread->m_regs[reg].AssignValue(value, false);
    }

    HYP_FORCE_INLINE void LoadU32(BCRegister reg, uint32 u32)
    {
        // get register value given
        Value value;
        value.m_type = Value::U32;
        value.m_value.u32 = u32;

        thread->m_regs[reg].AssignValue(value, false);
    }

    HYP_FORCE_INLINE void LoadU64(BCRegister reg, uint64 u64)
    {
        // get register value given
        Value value;
        value.m_type = Value::U64;
        value.m_value.u64 = u64;

        thread->m_regs[reg].AssignValue(value, false);
    }

    HYP_FORCE_INLINE void LoadF32(BCRegister reg, float f32)
    {
        // get register value given
        Value value;
        value.m_type = Value::F32;
        value.m_value.f = f32;

        thread->m_regs[reg].AssignValue(value, false);
    }

    HYP_FORCE_INLINE void LoadF64(BCRegister reg, double f64)
    {
        // get register value given
        Value value;
        value.m_type = Value::F64;
        value.m_value.d = f64;

        thread->m_regs[reg].AssignValue(value, false);
    }

    HYP_FORCE_INLINE void LoadOffset(BCRegister reg, uint16 offset)
    {
        const auto& stk = state->MAIN_THREAD->m_stack;

        Assert(
            offset <= stk.GetStackPointer(),
            "Stack offset out of bounds (%u)",
            offset);

        // read value from stack at (sp - offset)
        // into the the register
        thread->m_regs[reg].AssignValue(thread->m_stack[thread->m_stack.GetStackPointer() - offset], false);
    }

    HYP_FORCE_INLINE void LoadIndex(BCRegister reg, uint16 index)
    {
        const auto& stk = state->MAIN_THREAD->m_stack;

        Assert(
            index < stk.GetStackPointer(),
            "Stack index out of bounds (%u >= %llu)",
            index,
            stk.GetStackPointer());

        // read value from stack at the index into the the register
        // NOTE: read from main thread
        thread->m_regs[reg].AssignValue(stk[index], false);
    }

    HYP_FORCE_INLINE void LoadStatic(BCRegister reg, uint16 index)
    {
        // read value from static memory
        // at the index into the the register
        thread->m_regs[reg].AssignValue(state->m_staticMemory[index], false);
    }

    HYP_FORCE_INLINE void LoadConstantString(BCRegister reg, uint32 len, const char* str)
    {
        if (HeapValue* hv = state->HeapAlloc(thread))
        {
            hv->Assign(VMString(str));

            // assign register value to the allocated object
            Value v;
            v.m_type = Value::HEAP_POINTER;
            v.m_value.internal.ptr = hv;

            thread->m_regs[reg].AssignValue(v, false);

            hv->Mark();
        }
    }

    HYP_FORCE_INLINE void LoadAddr(BCRegister reg, BCAddress addr)
    {
        Value v;
        v.m_type = Value::ADDRESS;
        v.m_value.internal.addr = addr;

        thread->m_regs[reg].AssignValue(v, false);
    }

    HYP_FORCE_INLINE void LoadFunc(
        BCRegister reg,
        BCAddress addr,
        uint8 nargs,
        uint8 flags)
    {
        Value v;
        v.m_type = Value::FUNCTION;
        v.m_value.internal.func.m_addr = addr;
        v.m_value.internal.func.m_nargs = nargs;
        v.m_value.internal.func.m_flags = flags;

        thread->m_regs[reg].AssignValue(v, false);
    }

    HYP_FORCE_INLINE void LoadType(
        BCRegister reg,
        uint16 typeNameLen,
        const char* typeName,
        uint16 size,
        char** names)
    {
        // allocate heap value
        HeapValue* hv = state->HeapAlloc(thread);
        Assert(hv != nullptr);

        // create members
        Member* members = new Member[size];

        for (uint16 i = 0; i < size; i++)
        {
            Memory::StrCpy(members[i].name, names[i], sizeof(Member::name));
            members[i].hash = hashFnv1(names[i]);
            members[i].value = Value(Value::HEAP_POINTER, { .internal = { .ptr = nullptr } });
        }

        Value parentClassValue = thread->m_regs[reg];
        Assert(parentClassValue.GetType() == Value::HEAP_POINTER,
            "Parent class for type %s must be a heap pointer, got %s",
            typeName,
            parentClassValue.GetTypeString());

        // create prototype object
        hv->Assign(VMObject(members, size, parentClassValue.m_value.internal.ptr));

        delete[] members;

        // assign register value to the allocated object
        Value v;
        v.m_type = Value::HEAP_POINTER;
        v.m_value.internal.ptr = hv;

        thread->m_regs[reg].AssignValue(v, false);

        hv->Mark();
    }

    HYP_FORCE_INLINE void LoadMem(BCRegister dst, BCRegister src, uint8 index)
    {
        Value& sv = thread->m_regs[src];

        if (sv.m_type == Value::HEAP_POINTER)
        {
            HeapValue* hv = sv.m_value.internal.ptr;
            if (hv == nullptr)
            {
                state->ThrowException(
                    thread,
                    Exception::NullReferenceException());
                return;
            }
            else if (VMObject* objPtr = hv->GetPointer<VMObject>())
            {
                Assert(
                    index < objPtr->GetSize(),
                    "Index out of bounds (%u >= %llu)",
                    index,
                    objPtr->GetSize());
                thread->m_regs[dst].AssignValue(objPtr->GetMember(index).value, false);
                return;
            }
        }

        state->ThrowException(
            thread,
            Exception("Cannot access member by index: Not an VMObject"));
    }

    HYP_FORCE_INLINE void LoadMemHash(BCRegister dstReg, BCRegister srcReg, uint32 hash)
    {
        Value& sv = thread->m_regs[srcReg];

        if (sv.m_type == Value::HEAP_POINTER)
        {
            HeapValue* hv = sv.m_value.internal.ptr;

            if (hv == nullptr)
            {
                state->ThrowException(
                    thread,
                    Exception::NullReferenceException());
                return;
            }
            else if (VMObject* object = hv->GetPointer<VMObject>())
            {
                if (Member* member = object->LookupMemberFromHash(hash))
                {
                    thread->m_regs[dstReg].AssignValue(member->value, false);
                }
                else
                {
                    state->ThrowException(
                        thread,
                        Exception::MemberNotFoundException(hash));
                }
                return;
            }
        }

        state->ThrowException(
            thread,
            Exception("Cannot access member by hash: Not an VMObject"));
    }

    HYP_FORCE_INLINE void LoadArrayIdx(BCRegister dstReg, BCRegister srcReg, BCRegister indexReg)
    {
        Value& sv = thread->m_regs[srcReg];

        if (sv.m_type != Value::HEAP_POINTER)
        {
            state->ThrowException(
                thread,
                Exception("Not an Array"));
            return;
        }

        HeapValue* ptr = sv.m_value.internal.ptr;

        if (ptr == nullptr)
        {
            state->ThrowException(
                thread,
                Exception::NullReferenceException());
            return;
        }

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

        VMArray* array = ptr->GetPointer<VMArray>();

        if (array == nullptr)
        {
            if (auto* memoryBuffer = ptr->GetPointer<VMMemoryBuffer>())
            {
                // load the uint8 in the memory buffer, store it as int32
                Value memoryBufferData;
                memoryBufferData.m_type = Value::I32;

                if (key.index.flags & Number::FLAG_SIGNED)
                {
                    if (static_cast<SizeType>(key.index.i) >= memoryBuffer->GetSize())
                    {
                        state->ThrowException(
                            thread,
                            Exception::OutOfBoundsException());

                        return;
                    }

                    if (key.index.i < 0)
                    {
                        // wrap around (python style)
                        key.index.i = static_cast<int64>(memoryBuffer->GetSize() + key.index.i);
                        if (key.index.i < 0 || static_cast<SizeType>(key.index.i) >= memoryBuffer->GetSize())
                        {
                            state->ThrowException(
                                thread,
                                Exception::OutOfBoundsException());

                            return;
                        }
                    }

                    memoryBufferData.m_value.i32 = static_cast<int32>(static_cast<uint8*>(memoryBuffer->GetBuffer())[key.index.i]);
                }
                else if (key.index.flags & Number::FLAG_UNSIGNED)
                {
                    if (static_cast<SizeType>(key.index.u) >= memoryBuffer->GetSize())
                    {
                        state->ThrowException(
                            thread,
                            Exception::OutOfBoundsException());

                        return;
                    }

                    memoryBufferData.m_value.i32 = static_cast<int32>(static_cast<uint8*>(memoryBuffer->GetBuffer())[key.index.u]);
                }

                thread->m_regs[dstReg].AssignValue(memoryBufferData, false);

                return;
            }

            char buffer[256];
            std::snprintf(
                buffer,
                sizeof(buffer),
                "Expected Array or MemoryBuffer, got %s",
                sv.GetTypeString());

            state->ThrowException(
                thread,
                Exception(buffer));

            return;
        }

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

            thread->m_regs[dstReg].AssignValue(array->AtIndex(key.index.i), false);
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

            thread->m_regs[dstReg].AssignValue(array->AtIndex(key.index.u), false);
        }
    }

    HYP_FORCE_INLINE void LoadOffsetRef(BCRegister reg, uint16 offset)
    {
        thread->m_regs[reg] = Value(
            Value::ValueType::VALUE_REF,
            Value::ValueData { .internal = {.valueRef = &thread->m_stack[thread->m_stack.GetStackPointer() - offset] } });
    }

    HYP_FORCE_INLINE void LoadIndexRef(BCRegister reg, uint16 index)
    {
        auto& stk = state->MAIN_THREAD->m_stack;

        Assert(
            index < stk.GetStackPointer(),
            "Stack index out of bounds (%u >= %llu)",
            index,
            stk.GetStackPointer());

        thread->m_regs[reg] = Value(
            Value::ValueType::VALUE_REF,
            Value::ValueData { .internal = { .valueRef = &stk[index] } });
    }

    HYP_FORCE_INLINE void LoadRef(BCRegister dstReg, BCRegister srcReg)
    {
        Value value;
        value.m_type = Value::ValueType::VALUE_REF;
        value.m_value.internal.valueRef = &thread->m_regs[srcReg];

        thread->m_regs[dstReg] = value;
    }

    HYP_FORCE_INLINE void LoadDeref(BCRegister dstReg, BCRegister srcReg)
    {
        Value& src = thread->m_regs[srcReg];
        Assert(src.m_type == Value::VALUE_REF,
            "Value type must be VALUE_REF in order to deref, got %d", src.m_type);
        Assert(src.m_value.internal.valueRef != nullptr);

        Value deref = *src.m_value.internal.valueRef;

        thread->m_regs[dstReg] = deref;
    }

    HYP_FORCE_INLINE void LoadNull(BCRegister reg)
    {
        Value v;
        v.m_type = Value::HEAP_POINTER;
        v.m_value.internal.ptr = nullptr;

        thread->m_regs[reg].AssignValue(v, false);
    }

    HYP_FORCE_INLINE void LoadTrue(BCRegister reg)
    {
        thread->m_regs[reg].AssignValue(Value(Value::ValueType::BOOLEAN, { .b = true }), false);
    }

    HYP_FORCE_INLINE void LoadFalse(BCRegister reg)
    {
        thread->m_regs[reg].AssignValue(Value(Value::ValueType::BOOLEAN, { .b = false }), false);
    }

    HYP_FORCE_INLINE void MovOffset(uint16 offset, BCRegister reg)
    {
        // copy value from register to stack value at (sp - offset)
        thread->m_stack[thread->m_stack.GetStackPointer() - offset].AssignValue(thread->m_regs[reg], true);
    }

    HYP_FORCE_INLINE void MovIndex(uint16 index, BCRegister reg)
    {
        // copy value from register to stack value at index
        state->MAIN_THREAD->m_stack[index].AssignValue(thread->m_regs[reg], true);
    }

    HYP_FORCE_INLINE void MovStatic(uint16 index, BCRegister reg)
    {
        Assert(index < state->m_staticMemory.staticSize);

        // ensure we will not be overwriting something that is marked ALWAYS_ALIVE
        // will cause a memory leak if we overwrite it, or if we did change the flag,
        // it could cause corruption in places where we expect something to always exist.
        if (state->m_staticMemory[index].m_type == Value::HEAP_POINTER)
        {
            HeapValue* hv = state->m_staticMemory[index].m_value.internal.ptr;
            if (hv != nullptr && (hv->GetFlags() & GC_ALWAYS_ALIVE))
            {
                // state->ThrowException(
                //     thread,
                //     Exception("Cannot overwrite static value marked as ALWAYS_ALIVE")
                // );

                // return;

                // TEMP, hack
                hv->DisableFlags(GC_ALWAYS_ALIVE);
            }
        }

        // Mark our new value as ALWAYS_ALIVE if applicable.
        if (thread->m_regs[reg].m_type == Value::HEAP_POINTER)
        {
            if (HeapValue* hv = thread->m_regs[reg].m_value.internal.ptr)
            {
                thread->m_regs[reg].m_value.internal.ptr->EnableFlags(GC_ALWAYS_ALIVE);
            }
        }

        // copy value from register to static memory at index
        // moves to static do not impact refs
        state->m_staticMemory[index].AssignValue(thread->m_regs[reg], false);
    }

    HYP_FORCE_INLINE void MovMem(BCRegister dstReg, uint8 index, BCRegister srcReg)
    {
        Value& sv = thread->m_regs[dstReg];
        if (sv.m_type != Value::HEAP_POINTER)
        {
            state->ThrowException(
                thread,
                Exception("Cannot assign member by index: Not an VMObject"));
            return;
        }

        HeapValue* hv = sv.m_value.internal.ptr;
        if (hv == nullptr)
        {
            state->ThrowException(
                thread,
                Exception::NullReferenceException());
            return;
        }

        VMObject* object = hv->GetPointer<VMObject>();
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

        object->GetMember(index).value.AssignValue(thread->m_regs[srcReg], true);
        object->GetMember(index).value.Mark();
    }

    HYP_FORCE_INLINE void MovMemHash(BCRegister dstReg, uint32_t hash, BCRegister srcReg)
    {
        Value& sv = thread->m_regs[dstReg];

        if (sv.m_type != Value::HEAP_POINTER)
        {
            state->ThrowException(
                thread,
                Exception("Cannot assign member by hash: Not an VMObject"));
            return;
        }

        HeapValue* hv = sv.m_value.internal.ptr;
        if (hv == nullptr)
        {
            state->ThrowException(
                thread,
                Exception::NullReferenceException());
            return;
        }

        VMObject* object = hv->GetPointer<VMObject>();
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
        member->value.AssignValue(thread->m_regs[srcReg], true);
        member->value.Mark();
    }

    HYP_FORCE_INLINE void MovArrayIdx(BCRegister dstReg, uint32 index, BCRegister srcReg)
    {
        Value& sv = thread->m_regs[dstReg];

        if (sv.m_type != Value::HEAP_POINTER)
        {
            char buffer[256];
            std::snprintf(
                buffer,
                sizeof(buffer),
                "Expected Array or MemoryBuffer, got %s",
                sv.GetTypeString());

            state->ThrowException(
                thread,
                Exception(buffer));
            return;
        }

        HeapValue* hv = sv.m_value.internal.ptr;
        if (hv == nullptr)
        {
            state->ThrowException(
                thread,
                Exception::NullReferenceException());
            return;
        }

        VMArray* array = hv->GetPointer<VMArray>();

        if (array == nullptr)
        {
            if (auto* memoryBuffer = hv->GetPointer<VMMemoryBuffer>())
            {
                if (static_cast<SizeType>(index) >= memoryBuffer->GetSize())
                {
                    state->ThrowException(
                        thread,
                        Exception::OutOfBoundsException());

                    return;
                }

                if (index < 0)
                {
                    // wrap around (python style)
                    index = static_cast<int64>(memoryBuffer->GetSize() + static_cast<SizeType>(index));
                    if (index < 0 || static_cast<SizeType>(index) >= memoryBuffer->GetSize())
                    {
                        state->ThrowException(
                            thread,
                            Exception::OutOfBoundsException());
                        return;
                    }
                }

                Number dstData;

                if (!thread->m_regs[srcReg].GetSignedOrUnsigned(&dstData))
                {
                    state->ThrowException(
                        thread,
                        Exception::InvalidArgsException("integer"));

                    return;
                }

                ubyte byteValue = 0x0;

                // take the passed int/uint value and clip it to the first byte.
                if (dstData.flags & Number::FLAG_SIGNED)
                {
                    // convert -128..127 to 0..255 for signed integers.
                    byteValue = static_cast<ubyte>((dstData.i + 128) & 0xff);
                }
                else if (dstData.flags & Number::FLAG_UNSIGNED)
                {
                    byteValue = static_cast<ubyte>(dstData.u & 0xff);
                }
                else
                {
                    Assert(false, "Should not reach!");
                }

                // copy first byte from value
                Memory::MemCpy(static_cast<ubyte*>(memoryBuffer->GetBuffer()) + index, &byteValue, sizeof(ubyte));

                return;
            }

            char buffer[256];
            std::snprintf(
                buffer,
                sizeof(buffer),
                "Expected Array or MemoryBuffer, got %s",
                sv.GetTypeString());

            state->ThrowException(
                thread,
                Exception(buffer));

            return;
        }

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

        array->AtIndex(index) = thread->m_regs[srcReg];
        array->AtIndex(index).Mark();
    }

    HYP_FORCE_INLINE void MovArrayIdxReg(BCRegister dstReg, BCRegister indexReg, BCRegister srcReg)
    {
        Value& sv = thread->m_regs[dstReg];

        if (sv.m_type != Value::HEAP_POINTER)
        {
            char buffer[256];
            std::snprintf(
                buffer,
                sizeof(buffer),
                "Expected Array or MemoryBuffer, got %s",
                sv.GetTypeString());

            state->ThrowException(
                thread,
                Exception(buffer));

            return;
        }

        HeapValue* hv = sv.m_value.internal.ptr;
        if (hv == nullptr)
        {
            state->ThrowException(
                thread,
                Exception::NullReferenceException());
            return;
        }

        VMArray* array = hv->GetPointer<VMArray>();

        Number index;
        Value& indexRegisterValue = thread->m_regs[indexReg];

        if (!indexRegisterValue.GetSignedOrUnsigned(&index))
        {
            state->ThrowException(
                thread,
                Exception::InvalidArgsException("integer"));

            return;
        }

        if (array == nullptr)
        {
            if (auto* memoryBuffer = hv->GetPointer<VMMemoryBuffer>())
            {
                Number dstData;

                if (!thread->m_regs[srcReg].GetSignedOrUnsigned(&dstData))
                {
                    state->ThrowException(
                        thread,
                        Exception::InvalidArgsException("integer"));

                    return;
                }

                if (index.flags & Number::FLAG_SIGNED)
                {
                    int64 indexValue = index.i;

                    if (static_cast<SizeType>(indexValue) >= memoryBuffer->GetSize())
                    {
                        state->ThrowException(
                            thread,
                            Exception::OutOfBoundsException());

                        return;
                    }

                    if (indexValue < 0)
                    {
                        // wrap around (python style)
                        indexValue = static_cast<int64>(memoryBuffer->GetSize() + static_cast<SizeType>(indexValue));
                        if (indexValue < 0 || static_cast<SizeType>(indexValue) >= memoryBuffer->GetSize())
                        {
                            state->ThrowException(
                                thread,
                                Exception::OutOfBoundsException());
                            return;
                        }
                    }

                    Memory::MemCpy(&static_cast<uint8*>(memoryBuffer->GetBuffer())[indexValue], &dstData, sizeof(dstData));
                }
                else
                { // unsigned
                    const uint64 indexValue = index.u;

                    if (static_cast<SizeType>(indexValue) >= memoryBuffer->GetSize())
                    {
                        state->ThrowException(
                            thread,
                            Exception::OutOfBoundsException());

                        return;
                    }

                    Memory::MemCpy(&static_cast<uint8*>(memoryBuffer->GetBuffer())[indexValue], &dstData, sizeof(dstData));
                }

                return;
            }

            char buffer[256];
            std::snprintf(
                buffer,
                sizeof(buffer),
                "Expected Array or MemoryBuffer, got %s",
                indexRegisterValue.GetTypeString());

            state->ThrowException(
                thread,
                Exception(buffer));

            return;
        }

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

            array->AtIndex(indexValue) = thread->m_regs[srcReg];
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

            array->AtIndex(indexValue) = thread->m_regs[srcReg];
            array->AtIndex(indexValue).Mark();
        }
    }

    HYP_FORCE_INLINE void MovReg(BCRegister dstReg, BCRegister srcReg)
    {
        thread->m_regs[dstReg] = thread->m_regs[srcReg];
    }

    HYP_FORCE_INLINE void HasMemHash(BCRegister dstReg, BCRegister srcReg, uint32 hash)
    {
        Value& src = thread->m_regs[srcReg];

        Value result;
        result.m_value.b = false;
        result.m_type = Value::BOOLEAN;

        VMObject* ptr = nullptr;

        if (src.GetPointer<VMObject>(&ptr))
        {
            result.m_value.b = (ptr->LookupMemberFromHash(hash) != nullptr);
        }

        thread->m_regs[dstReg] = result;
    }

    HYP_FORCE_INLINE void Push(BCRegister reg)
    {
        // push a copy of the register value to the top of the stack
        thread->m_stack.Push(thread->m_regs[reg]);
    }

    HYP_FORCE_INLINE void Pop()
    {
        thread->m_stack.Pop();
    }

    HYP_FORCE_INLINE void PushArray(BCRegister dstReg, BCRegister srcReg)
    {
        Value& dst = thread->m_regs[dstReg];
        if (dst.m_type != Value::HEAP_POINTER)
        {
            state->ThrowException(
                thread,
                Exception("Not an Array"));
            return;
        }

        HeapValue* hv = dst.m_value.internal.ptr;
        if (hv == nullptr)
        {
            state->ThrowException(
                thread,
                Exception::NullReferenceException());
            return;
        }

        VMArray* array = hv->GetPointer<VMArray>();
        if (array == nullptr)
        {
            state->ThrowException(
                thread,
                Exception("Not an Array"));
            return;
        }

        array->Push(thread->m_regs[srcReg]);
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
        state->m_vm->Invoke(this, thread->m_regs[reg], nargs);
    }

    HYP_FORCE_INLINE void Ret()
    {
        // get top of stack (should be the address before jumping)
        Value& top = thread->GetStack().Top();
        Assert(top.GetType() == Value::FUNCTION_CALL);

        auto& callInfo = top.GetValue().internal.call;

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
        Value info;
        info.m_type = Value::TRY_CATCH_INFO;
        info.m_value.internal.tryCatchInfo.catchAddress = addr;

        // store the info
        thread->m_stack.Push(info);
    }

    HYP_FORCE_INLINE void EndTry()
    {
        // pop the try catch info from the stack
        Assert(thread->m_stack.Top().m_type == Value::TRY_CATCH_INFO);
        Assert(thread->m_exceptionState.m_tryCounter != 0);

        // pop try catch info
        thread->m_stack.Pop();
        --thread->m_exceptionState.m_tryCounter;
    }

    HYP_FORCE_INLINE void New(BCRegister dst, BCRegister src)
    {
        // read value from register
        Value& classSv = thread->m_regs[src];
        Assert(classSv.m_type == Value::HEAP_POINTER, "NEW operand must be a pointer type (%d), got %d", Value::HEAP_POINTER, classSv.m_type);

        HeapValue* classPtr = classSv.m_value.internal.ptr;
        Assert(classPtr != nullptr);

        Array<Span<Member>> memberSpans;

        HeapValue* basePtr = classPtr;
        uint32 depth = 0;

        Value& res = thread->m_regs[dst];

        while (basePtr != nullptr)
        {
            // @TODO Depth limit

            // the NEW instruction makes a copy of the $proto data member
            // of the prototype object.
            VMObject* baseVmObject = basePtr->GetPointer<VMObject>();
            Assert(baseVmObject != nullptr, "NEW operand should be a VMObject");

            Member* protoMem = baseVmObject->LookupMemberFromHash(VMObject::PROTO_MEMBER_HASH, false);

            if (!protoMem)
            {
                // This base class does not have a prototype member.
                break;
            }

            if (protoMem->value.m_type != Value::HEAP_POINTER)
            {
                if (depth == 0)
                {
                    // simply copy the value into the new value as it is not a heap pointer.
                    res.m_type = protoMem->value.m_type;
                    res.m_value = protoMem->value.m_value;
                }
                else
                {
                    state->ThrowException(
                        thread,
                        Exception::InvalidConstructorException());
                }

                return;
            }

            if (protoMem->value.m_value.internal.ptr == nullptr)
            {
                break;
            }

            VMObject* protoMemberObject = nullptr;

            if (!(protoMemberObject = protoMem->value.m_value.internal.ptr->GetPointer<VMObject>()))
            {
                if (depth == 0)
                {
                    // simply copy the value into the new value as it is not a heap pointer.
                    res.m_type = protoMem->value.m_type;
                    res.m_value = protoMem->value.m_value;
                }
                else
                {
                    state->ThrowException(
                        thread,
                        Exception::InvalidConstructorException());
                }

                return;
            }

            memberSpans.PushBack({ protoMemberObject->GetMembers(), protoMemberObject->GetSize() });

            Member* baseMember = baseVmObject->LookupMemberFromHash(VMObject::BASE_MEMBER_HASH, false);

            if (baseMember)
            {
                Assert(baseMember->value.m_type == Value::HEAP_POINTER, "Base class must be pointer type (%d), got %d", Value::HEAP_POINTER, baseMember->value.m_type);

                basePtr = baseMember->value.m_value.internal.ptr;
            }
            else
            {
                basePtr = nullptr;
            }

            depth++;
        }

        // allocate heap value for new object
        HeapValue* hv = state->HeapAlloc(thread);
        Assert(hv != nullptr);

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

        hv->Assign(VMObject(allMembers.Data(), allMembers.Size(), classPtr));

        res.m_type = Value::HEAP_POINTER;
        res.m_value.internal.ptr = hv;

        hv->Mark();
    }

    HYP_FORCE_INLINE void NewArray(BCRegister dst, uint32_t size)
    {
        // allocate heap object
        HeapValue* hv = state->HeapAlloc(thread);
        Assert(hv != nullptr);

        hv->Assign(VMArray(size));

        // assign register value to the allocated object
        Value& sv = thread->m_regs[dst];
        sv.m_type = Value::HEAP_POINTER;
        sv.m_value.internal.ptr = hv;

        hv->Mark();
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
        else if (lhs->m_type == Value::BOOLEAN && rhs->m_type == Value::BOOLEAN)
        {
            thread->m_regs.m_flags = (lhs->m_value.b == rhs->m_value.b)
                ? EQUAL
                : ((lhs->m_value.b > rhs->m_value.b)
                          ? GREATER
                          : NONE);
        }
        else if (lhs->m_type == Value::HEAP_POINTER && rhs->m_type == Value::HEAP_POINTER)
        {
            const auto res = Value::CompareAsPointers(lhs, rhs);

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
        else if (lhs->m_type == Value::FUNCTION && rhs->m_type == Value::FUNCTION)
        {
            thread->m_regs.m_flags = Value::CompareAsFunctions(lhs, rhs);
        }
        else if (lhs->m_type == Value::NATIVE_FUNCTION && rhs->m_type == Value::NATIVE_FUNCTION)
        {
            thread->m_regs.m_flags = Value::CompareAsNativeFunctions(lhs, rhs);
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
        else if (lhs->m_type == Value::BOOLEAN)
        {
            thread->m_regs.m_flags = !lhs->m_value.b ? EQUAL : NONE;
        }
        else if (lhs->m_type == Value::HEAP_POINTER)
        {
            thread->m_regs.m_flags = !lhs->m_value.internal.ptr ? EQUAL : NONE;
        }
        else if (lhs->m_type == Value::FUNCTION)
        {
            // functions are never null
            thread->m_regs.m_flags = NONE;
        }
        else
        {
            char buffer[512];
            std::snprintf(
                buffer,
                512,
                "Cannot determine if type '%s' is non-zero",
                lhs->GetTypeString());
            state->ThrowException(
                thread,
                Exception(buffer));
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

        Value result;
        result.m_type = MATCH_TYPES(lhs->m_type, rhs->m_type);

        Number a, b;

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
        thread->m_regs[dstReg] = result;
    }

    HYP_FORCE_INLINE void Sub(
        BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Value* lhs = &thread->m_regs[lhsReg];
        Value* rhs = &thread->m_regs[rhsReg];

        Value result;
        result.m_type = MATCH_TYPES(lhs->m_type, rhs->m_type);

        Number a, b;

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
        thread->m_regs[dstReg] = result;
    }

    HYP_FORCE_INLINE void Mul(
        BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Value* lhs = &thread->m_regs[lhsReg];
        Value* rhs = &thread->m_regs[rhsReg];

        Value result;
        result.m_type = MATCH_TYPES(lhs->m_type, rhs->m_type);

        Number a, b;

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
        thread->m_regs[dstReg] = result;
    }

    HYP_FORCE_INLINE void Div(
        BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Value* lhs = &thread->m_regs[lhsReg];
        Value* rhs = &thread->m_regs[rhsReg];

        Value result;
        result.m_type = MATCH_TYPES(lhs->m_type, rhs->m_type);

        Number a, b;

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            switch (result.m_type)
            {
            case Value::I8:
                if (b.i == 0)
                {
                    state->ThrowException(thread, Exception::DivisionByZeroException());
                    break;
                }
                result.m_value.i8 = static_cast<int8>(a.i / b.i);
                break;
            case Value::I16:
                if (b.i == 0)
                {
                    state->ThrowException(thread, Exception::DivisionByZeroException());
                    break;
                }
                result.m_value.i16 = static_cast<int16>(a.i / b.i);
                break;
            case Value::I32:
                if (b.i == 0)
                {
                    state->ThrowException(thread, Exception::DivisionByZeroException());
                    break;
                }
                result.m_value.i32 = static_cast<int32>(a.i / b.i);
                break;
            case Value::I64:
                if (b.i == 0)
                {
                    state->ThrowException(thread, Exception::DivisionByZeroException());
                    break;
                }
                result.m_value.i64 = a.i / b.i;
                break;
            case Value::U8:
                if (a.flags & Number::FLAG_SIGNED)
                {
                    result.m_value.u8 = static_cast<uint8>(a.i);
                }
                else
                {
                    result.m_value.u8 = static_cast<uint8>(a.u);
                }
                if (b.flags & Number::FLAG_SIGNED)
                {
                    if (b.i == 0)
                    {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.u8 = result.m_value.u8 / static_cast<uint8>(b.i);
                }
                else
                {
                    if (b.u == 0)
                    {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.u8 = result.m_value.u8 / static_cast<uint8>(b.u);
                }
                break;
            case Value::U16:
                if (a.flags & Number::FLAG_SIGNED)
                {
                    result.m_value.u16 = static_cast<uint16>(a.i);
                }
                else
                {
                    result.m_value.u16 = static_cast<uint16>(a.u);
                }
                if (b.flags & Number::FLAG_SIGNED)
                {
                    if (b.i == 0)
                    {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.u16 = result.m_value.u16 / static_cast<uint16>(b.i);
                }
                else
                {
                    if (b.u == 0)
                    {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.u16 = result.m_value.u16 / static_cast<uint16>(b.u);
                }
                break;
            case Value::U32:
                if (a.flags & Number::FLAG_SIGNED)
                {
                    result.m_value.u32 = static_cast<uint32>(a.i);
                }
                else
                {
                    result.m_value.u32 = static_cast<uint32>(a.u);
                }
                if (b.flags & Number::FLAG_SIGNED)
                {
                    if (b.i == 0)
                    {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.u32 = result.m_value.u32 / static_cast<uint32>(b.i);
                }
                else
                {
                    if (b.u == 0)
                    {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.u32 = result.m_value.u32 / static_cast<uint32>(b.u);
                }
                break;
            case Value::U64:
                if (a.flags & Number::FLAG_SIGNED)
                {
                    result.m_value.u64 = static_cast<uint64>(a.i);
                }
                else
                {
                    result.m_value.u64 = a.u;
                }
                if (b.flags & Number::FLAG_SIGNED)
                {
                    if (b.i == 0)
                    {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.u64 = result.m_value.u64 / static_cast<uint32>(b.i);
                }
                else
                {
                    if (b.u == 0)
                    {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.u64 = result.m_value.u64 / b.u;
                }
                break;
            case Value::F32:
                if (a.flags & Number::FLAG_SIGNED)
                {
                    result.m_value.f = static_cast<float>(a.i);
                }
                else if (a.flags & Number::FLAG_UNSIGNED)
                {
                    result.m_value.f = static_cast<float>(a.u);
                }
                else
                {
                    result.m_value.f = static_cast<float>(a.f);
                }
                if (b.flags & Number::FLAG_SIGNED)
                {
                    result.m_value.f = result.m_value.f / static_cast<float>(b.i);
                }
                else if (a.flags & Number::FLAG_UNSIGNED)
                {
                    result.m_value.f = result.m_value.f / static_cast<float>(b.u);
                }
                else
                {
                    result.m_value.f = result.m_value.f / static_cast<float>(b.f);
                }
                break;
            case Value::F64:
                if (a.flags & Number::FLAG_SIGNED)
                {
                    result.m_value.d = static_cast<double>(a.i);
                }
                else if (a.flags & Number::FLAG_UNSIGNED)
                {
                    result.m_value.d = static_cast<double>(a.u);
                }
                else
                {
                    result.m_value.d = a.f;
                }
                if (b.flags & Number::FLAG_SIGNED)
                {
                    result.m_value.d = result.m_value.d / static_cast<double>(b.i);
                }
                else if (a.flags & Number::FLAG_UNSIGNED)
                {
                    result.m_value.d = result.m_value.d / static_cast<double>(b.u);
                }
                else
                {
                    result.m_value.d = result.m_value.d / b.f;
                }
                break;
            default:
                Assert(false, "Result type was not a number. Investigate");
                break;
            }
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
        thread->m_regs[dstReg] = result;
    }

    HYP_FORCE_INLINE void Mod(
        BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Value* lhs = &thread->m_regs[lhsReg];
        Value* rhs = &thread->m_regs[rhsReg];

        Value result;
        result.m_type = MATCH_TYPES(lhs->m_type, rhs->m_type);

        Number a, b;

        if (lhs->GetNumber(&a) && rhs->GetNumber(&b))
        {
            switch (result.m_type)
            {
            case Value::I8:
                if (b.i == 0)
                {
                    state->ThrowException(thread, Exception::DivisionByZeroException());
                    break;
                }
                result.m_value.i8 = static_cast<int8>(a.i % b.i);
                break;
            case Value::I16:
                if (b.i == 0)
                {
                    state->ThrowException(thread, Exception::DivisionByZeroException());
                    break;
                }
                result.m_value.i16 = static_cast<int16>(a.i % b.i);
                break;
            case Value::I32:
                if (b.i == 0)
                {
                    state->ThrowException(thread, Exception::DivisionByZeroException());
                    break;
                }
                result.m_value.i32 = static_cast<int32>(a.i % b.i);
                break;
            case Value::I64:
                if (b.i == 0)
                {
                    state->ThrowException(thread, Exception::DivisionByZeroException());
                    break;
                }
                result.m_value.i64 = a.i % b.i;
                break;
            case Value::U8:
                if (a.flags & Number::FLAG_SIGNED)
                {
                    result.m_value.u8 = static_cast<uint8>(a.i);
                }
                else
                {
                    result.m_value.u8 = static_cast<uint8>(a.u);
                }
                if (b.flags & Number::FLAG_SIGNED)
                {
                    if (b.i == 0)
                    {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.u8 = result.m_value.u8 % static_cast<uint8>(b.i);
                }
                else
                {
                    if (b.u == 0)
                    {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.u8 = result.m_value.u8 % static_cast<uint8>(b.u);
                }
                break;
            case Value::U16:
                if (a.flags & Number::FLAG_SIGNED)
                {
                    result.m_value.u16 = static_cast<uint16>(a.i);
                }
                else
                {
                    result.m_value.u16 = static_cast<uint16>(a.u);
                }
                if (b.flags & Number::FLAG_SIGNED)
                {
                    if (b.i == 0)
                    {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.u16 = result.m_value.u16 % static_cast<uint16>(b.i);
                }
                else
                {
                    if (b.u == 0)
                    {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.u16 = result.m_value.u16 % static_cast<uint16>(b.u);
                }
                break;
            case Value::U32:
                if (a.flags & Number::FLAG_SIGNED)
                {
                    result.m_value.u32 = static_cast<uint32>(a.i);
                }
                else
                {
                    result.m_value.u32 = static_cast<uint32>(a.u);
                }
                if (b.flags & Number::FLAG_SIGNED)
                {
                    if (b.i == 0)
                    {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.u32 = result.m_value.u32 % static_cast<uint32>(b.i);
                }
                else
                {
                    if (b.u == 0)
                    {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.u32 = result.m_value.u32 % static_cast<uint32>(b.u);
                }
                break;
            case Value::U64:
                if (a.flags & Number::FLAG_SIGNED)
                {
                    result.m_value.u64 = static_cast<uint64>(a.i);
                }
                else
                {
                    result.m_value.u64 = a.u;
                }
                if (b.flags & Number::FLAG_SIGNED)
                {
                    if (b.i == 0)
                    {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.u64 = result.m_value.u64 % static_cast<uint32>(b.i);
                }
                else
                {
                    if (b.u == 0)
                    {
                        state->ThrowException(thread, Exception::DivisionByZeroException());
                        break;
                    }
                    result.m_value.u64 = result.m_value.u64 % b.u;
                }
                break;
            case Value::F32:
                if (a.flags & Number::FLAG_SIGNED)
                {
                    result.m_value.f = static_cast<float>(a.i);
                }
                else if (a.flags & Number::FLAG_UNSIGNED)
                {
                    result.m_value.f = static_cast<float>(a.u);
                }
                else
                {
                    result.m_value.f = static_cast<float>(a.f);
                }
                if (b.flags & Number::FLAG_SIGNED)
                {
                    result.m_value.f = fmodf(result.m_value.f, static_cast<float>(b.i));
                }
                else if (a.flags & Number::FLAG_UNSIGNED)
                {
                    result.m_value.f = fmodf(result.m_value.f, static_cast<float>(b.u));
                }
                else
                {
                    result.m_value.f = fmodf(result.m_value.f, a.f);
                }
                break;
            case Value::F64:
                if (a.flags & Number::FLAG_SIGNED)
                {
                    result.m_value.d = static_cast<double>(a.i);
                }
                else if (a.flags & Number::FLAG_UNSIGNED)
                {
                    result.m_value.d = static_cast<double>(a.u);
                }
                else
                {
                    result.m_value.d = a.f;
                }
                if (b.flags & Number::FLAG_SIGNED)
                {
                    result.m_value.d = std::fmod(result.m_value.d, static_cast<double>(b.i));
                }
                else if (a.flags & Number::FLAG_UNSIGNED)
                {
                    result.m_value.d = std::fmod(result.m_value.d, static_cast<double>(b.u));
                }
                else
                {
                    result.m_value.d = std::fmod(result.m_value.d, a.f);
                }
                break;
            default:
                Assert(false, "Invalid type, should not reach this state.");
                break;
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
        thread->m_regs[dstReg] = result;
    }

    HYP_FORCE_INLINE void And(
        BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Value* lhs = &thread->m_regs[lhsReg];
        Value* rhs = &thread->m_regs[rhsReg];

        Value result;
        result.m_type = MATCH_TYPES(lhs->m_type, rhs->m_type);

        Number a, b;

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
        thread->m_regs[dstReg] = result;
    }

    HYP_FORCE_INLINE void Or(
        BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Value* lhs = &thread->m_regs[lhsReg];
        Value* rhs = &thread->m_regs[rhsReg];

        Value result;
        result.m_type = MATCH_TYPES(lhs->m_type, rhs->m_type);

        Number a, b;

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
        thread->m_regs[dstReg] = result;
    }

    HYP_FORCE_INLINE void Xor(
        BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Value* lhs = &thread->m_regs[lhsReg];
        Value* rhs = &thread->m_regs[rhsReg];

        Value result;
        result.m_type = MATCH_TYPES(lhs->m_type, rhs->m_type);

        Number a, b;

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
        thread->m_regs[dstReg] = result;
    }

    HYP_FORCE_INLINE void Shl(BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Value* lhs = &thread->m_regs[lhsReg];
        Value* rhs = &thread->m_regs[rhsReg];

        Value result;
        result.m_type = MATCH_TYPES(lhs->m_type, rhs->m_type);

        Number a, b;

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
        thread->m_regs[dstReg] = result;
    }

    HYP_FORCE_INLINE void Shr(BCRegister lhsReg,
        BCRegister rhsReg,
        BCRegister dstReg)
    {
        // load values from registers
        Value* lhs = &thread->m_regs[lhsReg];
        Value* rhs = &thread->m_regs[rhsReg];

        Value result;
        result.m_type = MATCH_TYPES(lhs->m_type, rhs->m_type);

        Number a, b;

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
        thread->m_regs[dstReg] = result;
    }

    HYP_FORCE_INLINE void Not(BCRegister reg)
    {
        // load value from register
        Value* value = &thread->m_regs[reg];

        union
        {
            int64 i;
            uint64 u;
        };

        if (value->GetInteger(&i))
        {
            if (value->m_type == Value::I32)
            {
                value->m_value.i32 = static_cast<int32>(~i);
            }
            else if (value->m_type == Value::I16)
            {
                value->m_value.i16 = static_cast<int16>(~i);
            }
            else if (value->m_type == Value::I8)
            {
                value->m_value.i8 = static_cast<int8>(~i);
            }
            else
            {
                value->m_value.i64 = ~i;
            }
        }
        else if (value->GetUnsigned(&u))
        {
            if (value->m_type == Value::U32)
            {
                value->m_value.u32 = static_cast<uint32>(~u);
            }
            else if (value->m_type == Value::U16)
            {
                value->m_value.u16 = static_cast<uint16>(~u);
            }
            else if (value->m_type == Value::U8)
            {
                value->m_value.u8 = static_cast<uint8>(~u);
            }
            else
            {
                value->m_value.u64 = ~u;
            }
        }
        else
        {
            state->ThrowException(
                thread,
                Exception::InvalidBitwiseArgument());
            return;
        }
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
        Value* value = &thread->m_regs[reg];

        Number num;

        if (!value->GetNumber(&num))
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "NEG",
                    value->GetTypeString()));

            return;
        }

        if (num.flags & Number::FLAG_UNSIGNED)
        {
            if (value->m_type == Value::U32)
            {
                value->m_value.u32 = -static_cast<uint32>(num.u);
            }
            else if (value->m_type == Value::I16)
            {
                value->m_value.u16 = -static_cast<uint16>(num.u);
            }
            else if (value->m_type == Value::I8)
            {
                value->m_value.u8 = -static_cast<uint8>(num.u);
            }
            else
            {
                value->m_value.u64 = -num.u;
            }
        }
        else if (num.flags & Number::FLAG_SIGNED)
        {
            if (value->m_type == Value::I32)
            {
                value->m_value.i32 = -static_cast<int32>(num.i);
            }
            else if (value->m_type == Value::I16)
            {
                value->m_value.i16 = -static_cast<int16>(num.i);
            }
            else if (value->m_type == Value::I8)
            {
                value->m_value.i8 = -static_cast<int8>(num.i);
            }
            else
            {
                value->m_value.i64 = -num.i;
            }
        }
        else
        {
            if (value->m_type == Value::F32)
            {
                value->m_value.f = -static_cast<float>(num.f);
            }
            else
            {
                value->m_value.d = -num.f;
            }
        }
    }

    HYP_FORCE_INLINE void CastU8(BCRegister dst, BCRegister src)
    {
        // load value from register
        Value* value = &thread->m_regs[src];

        Number num;

        if (!value->GetNumber(&num))
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_U8",
                    value->GetTypeString()));

            return;
        }

        thread->m_regs[dst].m_type = Value::U8;

        if (num.flags & Number::FLAG_UNSIGNED)
        {
            thread->m_regs[dst].m_value.u8 = static_cast<uint8>(num.u);
        }
        else if (num.flags & Number::FLAG_SIGNED)
        {
            thread->m_regs[dst].m_value.u8 = static_cast<uint8>(num.i);
        }
        else
        {
            thread->m_regs[dst].m_value.u8 = static_cast<uint8>(num.f);
        }
    }

    HYP_FORCE_INLINE void CastU16(BCRegister dst, BCRegister src)
    {
        // load value from register
        Value* value = &thread->m_regs[src];

        Number num;

        if (!value->GetNumber(&num))
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_U16",
                    value->GetTypeString()));

            return;
        }

        thread->m_regs[dst].m_type = Value::U16;

        if (num.flags & Number::FLAG_UNSIGNED)
        {
            thread->m_regs[dst].m_value.u16 = static_cast<uint16>(num.u);
        }
        else if (num.flags & Number::FLAG_SIGNED)
        {
            thread->m_regs[dst].m_value.u16 = static_cast<uint16>(num.i);
        }
        else
        {
            thread->m_regs[dst].m_value.u16 = static_cast<uint16>(num.f);
        }
    }

    HYP_FORCE_INLINE void CastU32(BCRegister dst, BCRegister src)
    {
        // load value from register
        Value* value = &thread->m_regs[src];

        Number num;

        if (!value->GetNumber(&num))
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_U32",
                    value->GetTypeString()));

            return;
        }

        thread->m_regs[dst].m_type = Value::U32;

        if (num.flags & Number::FLAG_UNSIGNED)
        {
            thread->m_regs[dst].m_value.u32 = static_cast<uint32>(num.u);
        }
        else if (num.flags & Number::FLAG_SIGNED)
        {
            thread->m_regs[dst].m_value.u32 = static_cast<uint32>(num.i);
        }
        else
        {
            thread->m_regs[dst].m_value.u32 = static_cast<uint32>(num.f);
        }
    }

    HYP_FORCE_INLINE void CastU64(BCRegister dst, BCRegister src)
    {
        // load value from register
        Value* value = &thread->m_regs[src];

        Number num;

        if (!value->GetNumber(&num))
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_U64",
                    value->GetTypeString()));

            return;
        }

        thread->m_regs[dst].m_type = Value::U64;

        if (num.flags & Number::FLAG_UNSIGNED)
        {
            thread->m_regs[dst].m_value.u64 = num.u;
        }
        else if (num.flags & Number::FLAG_SIGNED)
        {
            thread->m_regs[dst].m_value.u64 = static_cast<uint64>(num.i);
        }
        else
        {
            thread->m_regs[dst].m_value.u64 = static_cast<uint64>(num.f);
        }
    }

    HYP_FORCE_INLINE void CastI8(BCRegister dst, BCRegister src)
    {
        // load value from register
        Value* value = &thread->m_regs[src];

        Number num;

        if (!value->GetNumber(&num))
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_I8",
                    value->GetTypeString()));

            return;
        }

        thread->m_regs[dst].m_type = Value::I8;

        if (num.flags & Number::FLAG_UNSIGNED)
        {
            thread->m_regs[dst].m_value.i8 = static_cast<int8>(num.u);
        }
        else if (num.flags & Number::FLAG_SIGNED)
        {
            thread->m_regs[dst].m_value.i8 = static_cast<int8>(num.i);
        }
        else
        {
            thread->m_regs[dst].m_value.i8 = static_cast<int8>(num.f);
        }
    }

    HYP_FORCE_INLINE void CastI16(BCRegister dst, BCRegister src)
    {
        // load value from register
        Value* value = &thread->m_regs[src];

        Number num;

        if (!value->GetNumber(&num))
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_I16",
                    value->GetTypeString()));

            return;
        }

        thread->m_regs[dst].m_type = Value::I16;

        if (num.flags & Number::FLAG_UNSIGNED)
        {
            thread->m_regs[dst].m_value.i16 = static_cast<int16>(num.u);
        }
        else if (num.flags & Number::FLAG_SIGNED)
        {
            thread->m_regs[dst].m_value.i16 = static_cast<int16>(num.i);
        }
        else
        {
            thread->m_regs[dst].m_value.i16 = static_cast<int16>(num.f);
        }
    }

    HYP_FORCE_INLINE void CastI32(BCRegister dst, BCRegister src)
    {
        // load value from register
        Value* value = &thread->m_regs[src];

        Number num;

        if (!value->GetNumber(&num))
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_I32",
                    value->GetTypeString()));

            return;
        }

        thread->m_regs[dst].m_type = Value::I32;

        if (num.flags & Number::FLAG_UNSIGNED)
        {
            thread->m_regs[dst].m_value.i32 = static_cast<int32>(num.u);
        }
        else if (num.flags & Number::FLAG_SIGNED)
        {
            thread->m_regs[dst].m_value.i32 = static_cast<int32>(num.i);
        }
        else
        {
            thread->m_regs[dst].m_value.i32 = static_cast<int32>(num.f);
        }
    }

    HYP_FORCE_INLINE void CastI64(BCRegister dst, BCRegister src)
    {
        // load value from register
        Value* value = &thread->m_regs[src];

        Number num;

        if (!value->GetNumber(&num))
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_I64",
                    value->GetTypeString()));

            return;
        }

        thread->m_regs[dst].m_type = Value::I64;

        if (num.flags & Number::FLAG_UNSIGNED)
        {
            thread->m_regs[dst].m_value.i64 = static_cast<int64>(num.u);
        }
        else if (num.flags & Number::FLAG_SIGNED)
        {
            thread->m_regs[dst].m_value.i64 = num.i;
        }
        else
        {
            thread->m_regs[dst].m_value.i64 = static_cast<int64>(num.f);
        }
    }

    HYP_FORCE_INLINE void CastF32(BCRegister dst, BCRegister src)
    {
        // load value from register
        Value* value = &thread->m_regs[src];

        Number num;

        if (!value->GetNumber(&num))
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_F32",
                    value->GetTypeString()));

            return;
        }

        thread->m_regs[dst].m_type = Value::F32;

        if (num.flags & Number::FLAG_UNSIGNED)
        {
            thread->m_regs[dst].m_value.f = static_cast<float>(num.u);
        }
        else if (num.flags & Number::FLAG_SIGNED)
        {
            thread->m_regs[dst].m_value.f = static_cast<float>(num.i);
        }
        else
        {
            thread->m_regs[dst].m_value.f = num.f;
        }
    }

    HYP_FORCE_INLINE void CastF64(BCRegister dst, BCRegister src)
    {
        // load value from register
        Value* value = &thread->m_regs[src];

        if (value->m_type == Value::F32)
        {
            thread->m_regs[dst].m_type = Value::F64;
            thread->m_regs[dst].m_value.d = static_cast<double>(value->m_value.f);
        }
        else
        {
            Number num;

            if (!value->GetNumber(&num))
            {
                state->ThrowException(
                    thread,
                    Exception::InvalidOperationException(
                        "CAST_F64",
                        value->GetTypeString()));

                return;
            }

            thread->m_regs[dst].m_type = Value::F64;

            if (num.flags & Number::FLAG_UNSIGNED)
            {
                thread->m_regs[dst].m_value.d = static_cast<double>(num.u);
            }
            else if (num.flags & Number::FLAG_SIGNED)
            {
                thread->m_regs[dst].m_value.d = static_cast<double>(num.i);
            }
            else
            {
                thread->m_regs[dst].m_value.d = num.f;
            }
        }
    }

    HYP_FORCE_INLINE void CastBool(BCRegister dst, BCRegister src)
    {
        // load value from register
        Value* value = &thread->m_regs[src];

        if (value->m_type == Value::BOOLEAN)
        {
            thread->m_regs[dst].m_type = Value::BOOLEAN;
            thread->m_regs[dst].m_value.b = value->m_value.b;
        }
        else
        {
            Number num;

            if (!value->GetNumber(&num))
            {
                state->ThrowException(
                    thread,
                    Exception::InvalidOperationException(
                        "CAST_BOOL",
                        value->GetTypeString()));

                return;
            }

            thread->m_regs[dst].m_type = Value::BOOLEAN;

            if (num.flags & Number::FLAG_UNSIGNED)
            {
                thread->m_regs[dst].m_value.b = static_cast<bool>(num.u);
            }
            else if (num.flags & Number::FLAG_SIGNED)
            {
                thread->m_regs[dst].m_value.b = static_cast<bool>(num.i);
            }
            else
            {
                thread->m_regs[dst].m_value.b = static_cast<bool>(num.f);
            }
        }
    }

    HYP_FORCE_INLINE void CastDynamic(BCRegister dst, BCRegister src)
    {
        // load the VMObject from dst
        Value* value = &thread->m_regs[dst];

        // Ensure it is a VMObject
        VMObject* classObjectPtr = nullptr;

        if (!value->GetPointer<VMObject>(&classObjectPtr))
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_DYNAMIC",
                    value->GetTypeString()));

            return;
        }

        // load the target from src
        Value* target = &thread->m_regs[src];

        // Ensure it is a VMObject
        VMObject* targetObjectPtr = nullptr;

        if (!target->GetPointer<VMObject>(&targetObjectPtr))
        {
            state->ThrowException(
                thread,
                Exception::InvalidOperationException(
                    "CAST_DYNAMIC",
                    target->GetTypeString()));

            return;
        }

        bool isInstance = false;

        // Check if the target is an instance of the type
        vm::Value base = vm::Value(vm::Value::NONE, { .internal = { .ptr = nullptr } });

        if (HeapValue* targetClass = targetObjectPtr->GetClassPointer())
        {
            constexpr uint32 maxDepth = 1024;
            uint32 depth = 0;

            vm::VMObject* targetClassObject = targetClass->GetPointer<vm::VMObject>();

            while (targetClassObject != nullptr && depth < maxDepth)
            {
                isInstance = (*targetClassObject == *classObjectPtr);

                if (isInstance)
                {
                    break;
                }

                if (!(targetClassObject->LookupBasePointer(&base) && base.GetPointer<vm::VMObject>(&targetClassObject)))
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

        // Set the destination register to be the target
        thread->m_regs[dst] = base;
    }
};

} // namespace vm
} // namespace hyperion

