#include <script/vm/VM.hpp>
#include <script/vm/Value.hpp>
#include <script/vm/HeapValue.hpp>
#include <script/vm/VMArray.hpp>
#include <script/vm/VMArraySlice.hpp>
#include <script/vm/VMObject.hpp>
#include <script/vm/VMString.hpp>
#include <script/vm/VMTypeInfo.hpp>
#include <script/vm/InstructionHandler.hpp>

#include <core/Types.hpp>
#include <script/Instructions.hpp>
#include <script/Hasher.hpp>
#include <core/debug/Debug.hpp>

#include <algorithm>
#include <cstdio>
#include <cinttypes>
#include <mutex>
#include <sstream>

namespace hyperion {
namespace vm {

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

        handler.ExportSymbol(
            reg,
            hash);

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

void VM::PushNativeFunctionPtr(NativeFunctionPtr_t ptr)
{
    Value sv;
    sv.m_type = Value::NATIVE_FUNCTION;
    sv.m_value.nativeFunc = ptr;

    Assert(m_state.GetMainThread() != nullptr);
    m_state.GetMainThread()->m_stack.Push(sv);
}

void VM::Invoke(
    InstructionHandler* handler,
    const Value& value,
    uint8 nargs)
{
    static const uint32 invokeHash = hashFnv1("$invoke");

    VMState* state = handler->state;
    ExecutionThread* thread = handler->thread;
    BytecodeStream* bs = handler->bs;

    Assert(state != nullptr);
    Assert(thread != nullptr);
    Assert(bs != nullptr);

    if (value.m_type != Value::FUNCTION)
    {
        if (value.m_type == Value::NATIVE_FUNCTION)
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
            value.m_value.nativeFunc(params);

            // re-enable auto gc
            state->enableAutoGc = ENABLE_GC;

            delete[] args;

            return;
        }
        else if (value.m_type == Value::HEAP_POINTER)
        {
            if (value.m_value.ptr == nullptr)
            {
                state->ThrowException(
                    thread,
                    Exception::NullReferenceException());
                return;
            }
            else if (VMObject* object = value.m_value.ptr->GetPointer<VMObject>())
            {
                if (Member* member = object->LookupMemberFromHash(invokeHash))
                {
                    const int64 sp = static_cast<int64>(thread->m_stack.GetStackPointer());
                    const int64 argsStart = sp - nargs;

                    if (nargs > 0)
                    {
                        // shift over by 1 -- and insert 'self' to start of args
                        // make a copy of last item to not overwrite it
                        thread->m_stack.Push(thread->m_stack[sp - 1]);

                        for (SizeType i = argsStart; i < sp - 1; i++)
                        {
                            thread->m_stack[i + 1] = thread->m_stack[i];
                        }

                        // set 'self' object to start of args
                        thread->m_stack[argsStart] = value;
                    }
                    else
                    {
                        thread->m_stack.Push(value);
                    }

                    Invoke(
                        handler,
                        member->value,
                        nargs + 1);

                    Value& top = thread->m_stack.Top();
                    Assert(top.m_type == Value::FUNCTION_CALL);

                    // bookkeeping to remove the closure object
                    // normally, arguments are popped after the call is returned,
                    // rather than within the body
                    top.m_value.call.varargsPush--;

                    return;
                }
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

    if ((value.m_value.func.m_flags & FunctionFlags::VARIADIC) && nargs < value.m_value.func.m_nargs - 1)
    {
        // if variadic, make sure the arg count is /at least/ what is required
        state->ThrowException(
            thread,
            Exception::InvalidArgsException(
                value.m_value.func.m_nargs,
                nargs,
                true));
    }
    else if (!(value.m_value.func.m_flags & FunctionFlags::VARIADIC) && value.m_value.func.m_nargs != nargs)
    {
        state->ThrowException(
            thread,
            Exception::InvalidArgsException(
                value.m_value.func.m_nargs,
                nargs));
    }
    else
    {
        Value previousAddr;
        previousAddr.m_type = Value::FUNCTION_CALL;
        previousAddr.m_value.call.varargsPush = 0;
        previousAddr.m_value.call.returnAddress = static_cast<BCAddress>(bs->Position());

        if (value.m_value.func.m_flags & FunctionFlags::VARIADIC)
        {
            // for each argument that is over the expected size, we must pop it from
            // the stack and add it to a new array.
            int varargsAmt = nargs - value.m_value.func.m_nargs + 1;
            if (varargsAmt < 0)
            {
                varargsAmt = 0;
            }

            // set varargsPush value so we know how to get back to the stack size before.
            previousAddr.m_value.call.varargsPush = varargsAmt - 1;

            // allocate heap object
            HeapValue* hv = state->HeapAlloc(thread);
            Assert(hv != nullptr);

            // create VMArray object to hold variadic args
            VMArray arr(varargsAmt);

            for (int i = varargsAmt - 1; i >= 0; i--)
            {
                // push to array
                arr.AtIndex(i, thread->GetStack().Top());
                thread->GetStack().Pop();
            }

            // assign heap value to our array
            hv->Assign(std::move(arr));

            Value arrayValue;
            arrayValue.m_type = Value::HEAP_POINTER;
            arrayValue.m_value.ptr = hv;

            hv->Mark();

            // push the array to the stack
            thread->GetStack().Push(arrayValue);
        }

        // push the address
        thread->GetStack().Push(previousAddr);
        // seek to the new address
        bs->Seek(value.m_value.func.m_addr);

        // increase function depth
        thread->m_funcDepth++;
    }
}

void VM::InvokeNow(
    BytecodeStream* bs,
    const Value& value,
    uint8 nargs)
{
    ExecutionThread* thread = m_state.MAIN_THREAD;

    const SizeType positionBefore = bs->Position();
    const uint32 originalFunctionDepth = thread->m_funcDepth;
    const SizeType stackSizeBefore = thread->GetStack().GetStackPointer();

    InstructionHandler handler(
        &m_state,
        thread,
        bs);

    Invoke(
        &handler,
        value,
        nargs);

    if (value.m_type == Value::FUNCTION)
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

void VM::CreateStackTrace(ExecutionThread* thread, StackTrace* out)
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

        if (top.m_type == Value::FUNCTION_CALL)
        {
            out->callAddresses[numRecordedCallAddresses++] = static_cast<int>(top.m_value.call.returnAddress);
        }
    }
}

bool VM::HandleException(InstructionHandler* handler)
{
    ExecutionThread* thread = handler->thread;
    BytecodeStream* bs = handler->bs;

    if (thread->m_exceptionState.m_tryCounter != 0)
    {
        // handle exception
        --thread->m_exceptionState.m_tryCounter;

        Assert(thread->m_exceptionState.m_exceptionDepth != 0);
        --thread->m_exceptionState.m_exceptionDepth;

        Value* top = nullptr;
        while ((top = &thread->m_stack.Top()) && top->m_type != Value::TRY_CATCH_INFO)
        {
            thread->m_stack.Pop();
        }

        // top should be exception data
        Assert(top != nullptr && top->m_type == Value::TRY_CATCH_INFO);

        // jump to the catch block
        bs->Seek(top->m_value.tryCatchInfo.catchAddress);

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
