#include <script/vm/Value.hpp>
#include <script/vm/VMObject.hpp>
#include <script/vm/VMArray.hpp>
#include <script/vm/VMMemoryBuffer.hpp>
#include <script/vm/VMArraySlice.hpp>
#include <script/vm/VMString.hpp>
#include <script/vm/VMMap.hpp>
#include <script/vm/HeapValue.hpp>
#include <script/Hasher.hpp>

#include <core/debug/Debug.hpp>

#include <stdio.h>
#include <cinttypes>
#include <iostream>

namespace hyperion {
namespace vm {

static const VMString NULL_STRING = VMString("null");
static const VMString BOOLEAN_STRINGS[2] = { VMString("false"), VMString("true") };

TypeId GetTypeIdForHeapValue(const HeapValue* heapValue)
{
    if (!heapValue)
    {
        return TypeId::ForType<void>();
    }

    return heapValue->GetTypeId();
}

void* GetRawPointerForHeapValue(HeapValue* heapValue)
{
    if (!heapValue)
    {
        return nullptr;
    }

    return heapValue->GetRawPointer();
}

const void* GetRawPointerForHeapValue(const HeapValue* heapValue)
{
    if (!heapValue)
    {
        return nullptr;
    }

    return heapValue->GetRawPointer();
}

Value::Value(const Value& other)
    : m_type(other.m_type),
      m_value(other.m_value)
{
}

Value::Value(ValueType valueType, ValueData valueData)
    : m_type(valueType),
      m_value(valueData)
{
}

int Value::CompareAsPointers(
    Value* lhs,
    Value* rhs)
{
    HeapValue* a = lhs->m_value.internal.ptr;
    HeapValue* b = rhs->m_value.internal.ptr;

    if (a == b)
    {
        // pointers equal, drop out early.
        return CompareFlags::EQUAL;
    }
    else if (a == nullptr || b == nullptr)
    {
        return CompareFlags::NONE;
    }
    else if (a->GetRawPointer() == b->GetRawPointer())
    {
        // pointers equal
        return CompareFlags::EQUAL;
    }
    else if (a->GetTypeId() != b->GetTypeId())
    {
        // type IDs not equal, drop out
        return CompareFlags::NONE;
    }
    else if (const VMObject* vmObject = a->GetPointer<VMObject>())
    {
        return *vmObject == b->Get<VMObject>()
            ? CompareFlags::EQUAL
            : CompareFlags::NONE;
    }
    else if (const VMString* vmString = a->GetPointer<VMString>())
    {
        return *vmString == b->Get<VMString>()
            ? CompareFlags::EQUAL
            : CompareFlags::NONE;
    }
    else if (const VMArray* vmArray = a->GetPointer<VMArray>())
    {
        return *vmArray == b->Get<VMArray>()
            ? CompareFlags::EQUAL
            : CompareFlags::NONE;
    }
    else
    {
        return CompareFlags::NONE;
    }
}

void Value::Mark()
{
    switch (m_type)
    {
    case Value::VALUE_REF:
        Assert(m_value.internal.valueRef != nullptr);

        if (m_value.internal.valueRef != this)
        {
            m_value.internal.valueRef->Mark();
        }

        break;

    case Value::HEAP_POINTER:
    {
        HeapValue* ptr = m_value.internal.ptr;

        if (ptr != nullptr)
        {
            Assert(!(ptr->GetFlags() & GC_DESTROYED), "VM heap corruption! VMObject had flag GC_DESTROYED in Mark()");

            if (!(ptr->GetFlags() & GC_ALIVE))
            {
                ptr->Mark();
            }
        }

        break;
    }

    default:
        break;
    }
}

const char* Value::GetTypeString() const
{
    switch (m_type)
    {
    case NONE:
        return "<Uninitialized data>";
    case I8:
        return "int8";
    case I16:
        return "int16";
    case I32:
        return "int32";
    case I64:
        return "int64";
    case U8:
        return "uint8";
    case U16:
        return "uint16";
    case U32:
        return "uint32";
    case U64:
        return "uint64";
    case F32:
        return "float";
    case F64:
        return "double";
    case BOOLEAN:
        return "bool";
    case VALUE_REF:
        Assert(m_value.internal.valueRef != nullptr);

        if (m_value.internal.valueRef != this)
        {
            return m_value.internal.valueRef->GetTypeString();
        }
        else
        {
            return "<Circular Reference>";
        }

    case HEAP_POINTER:
        if (m_value.internal.ptr == nullptr)
        {
            return "Null";
        }
        else if (m_value.internal.ptr->GetPointer<VMString>())
        {
            return "String";
        }
        else if (m_value.internal.ptr->GetPointer<VMArray>() || m_value.internal.ptr->GetPointer<VMArraySlice>())
        {
            return "Array";
        }
        else if (m_value.internal.ptr->GetPointer<VMMemoryBuffer>())
        {
            return "MemoryBuffer";
        }
        else if (m_value.internal.ptr->GetPointer<VMStruct>())
        {
            return "Struct";
        }
        else if (VMObject* object = m_value.internal.ptr->GetPointer<VMObject>())
        {
            return "Object"; // TODO prototype name
        }

        return "<Unknown pointer type>";

    case FUNCTION: // fallthrough
    case NATIVE_FUNCTION:
        return "Function";
    case ADDRESS:
        return "<Function address>";
    case FUNCTION_CALL:
        return "<Stack frame>";
    case TRY_CATCH_INFO:
        return "<Try catch info>";
    case USER_DATA:
        return "UserData";
    default:
        return "<Invalid type>";
    }
}

VMString Value::ToString() const
{
    const SizeType bufSize = 256;
    char buf[bufSize] = { 0 };

    const int depth = 3;

    switch (m_type)
    {
    case Value::I8:
    {
        int n = snprintf(buf, bufSize, "%" PRId8, m_value.i8);
        return VMString(buf, n);
    }
    case Value::I16:
    {
        int n = snprintf(buf, bufSize, "%" PRId16, m_value.i16);
        return VMString(buf, n);
    }
    case Value::I32:
    {
        int n = snprintf(buf, bufSize, "%" PRId32, m_value.i32);
        return VMString(buf, n);
    }
    case Value::I64:
    {
        int n = snprintf(buf, bufSize, "%" PRId64, m_value.i64);
        return VMString(buf, n);
    }

    case Value::U8:
    {
        int n = snprintf(buf, bufSize, "%" PRIu8, m_value.u8);
        return VMString(buf, n);
    }
    case Value::U16:
    {
        int n = snprintf(buf, bufSize, "%" PRIu16, m_value.u16);
        return VMString(buf, n);
    }
    case Value::U32:
    {
        int n = snprintf(buf, bufSize, "%" PRIu32, m_value.u32);
        return VMString(buf, n);
    }
    case Value::U64:
    {
        int n = snprintf(buf, bufSize, "%" PRIu64, m_value.u64);
        return VMString(buf, n);
    }
    case Value::F32:
    {
        int n = snprintf(
            buf,
            bufSize,
            "%g",
            m_value.f);
        return VMString(buf, n);
    }

    case Value::F64:
    {
        int n = snprintf(
            buf,
            bufSize,
            "%g",
            m_value.d);
        return VMString(buf, n);
    }

    case Value::BOOLEAN:
        return BOOLEAN_STRINGS[m_value.b];

    case Value::VALUE_REF:
        Assert(m_value.internal.valueRef != nullptr);
        if (m_value.internal.valueRef == this)
        {
            return VMString("<Circular Reference>");
        }
        else
        {
            return m_value.internal.valueRef->ToString();
        }
    case Value::USER_DATA:
    {
        int n = snprintf(buf, bufSize, "%p", m_value.internal.userData);
        return VMString(buf, n);
    }
    case Value::HEAP_POINTER:
    {
        if (m_value.internal.ptr == nullptr)
        {
            return NULL_STRING;
        }
        else if (VMString* string = m_value.internal.ptr->GetPointer<VMString>())
        {
            return *string;
        }
        else if (VMArray* array = m_value.internal.ptr->GetPointer<VMArray>())
        {
            std::stringstream ss;
            array->GetRepresentation(ss, true, depth);
            const std::string& str = ss.str();
            return VMString(str.c_str());
        }
        else if (VMMemoryBuffer* memoryBuffer = m_value.internal.ptr->GetPointer<VMMemoryBuffer>())
        {
            std::stringstream ss;
            memoryBuffer->GetRepresentation(ss, true, depth);
            const std::string& str = ss.str();
            return VMString(str.c_str());
        }
        else if (VMArraySlice* slice = m_value.internal.ptr->GetPointer<VMArraySlice>())
        {
            std::stringstream ss;
            slice->GetRepresentation(ss, true, depth);
            const std::string& str = ss.str();
            return VMString(str.c_str());
        }
        else if (VMObject* object = m_value.internal.ptr->GetPointer<VMObject>())
        {
            std::stringstream ss;
            object->GetRepresentation(ss, true, depth);
            const std::string& str = ss.str();
            return VMString(str.c_str());
        }
        else if (VMMap* map = m_value.internal.ptr->GetPointer<VMMap>())
        {
            std::stringstream ss;
            map->GetRepresentation(ss, true, depth);
            const std::string& str = ss.str();
            return VMString(str.c_str());
        }
        else
        {
            // return memory address as string
            int n = snprintf(buf, bufSize, "%p", (void*)m_value.internal.ptr);
            return VMString(buf, n);
        }

        break;
    }

    default:
        return VMString(GetTypeString());
    }
}

void Value::ToRepresentation(
    std::stringstream& ss,
    bool addTypeName,
    int depth) const
{
    switch (m_type)
    {
    case Value::VALUE_REF:
        Assert(m_value.internal.valueRef != nullptr);

        if (m_value.internal.valueRef == this)
        {
            ss << "<Circular Reference>";
        }
        else
        {
            m_value.internal.valueRef->ToRepresentation(
                ss,
                addTypeName,
                depth);
        }

        return;

    case Value::HEAP_POINTER:
        if (m_value.internal.ptr == nullptr)
        {
            ss << "null";
        }
        else if (VMString* string = m_value.internal.ptr->GetPointer<VMString>())
        {
            ss << '\"';
            ss << string->GetData();
            ss << '\"';
        }
        else if (VMArray* array = m_value.internal.ptr->GetPointer<VMArray>())
        {
            array->GetRepresentation(ss, addTypeName, depth);
        }
        else if (VMArraySlice* slice = m_value.internal.ptr->GetPointer<VMArraySlice>())
        {
            slice->GetRepresentation(ss, addTypeName, depth);
        }
        else if (VMObject* object = m_value.internal.ptr->GetPointer<VMObject>())
        {
            object->GetRepresentation(ss, addTypeName, depth);
        }
        else if (VMMap* map = m_value.internal.ptr->GetPointer<VMMap>())
        {
            map->GetRepresentation(ss, addTypeName, depth);
        }
        else
        {
            if (addTypeName)
            {
                ss << GetTypeString();
                ss << '(';
            }

            ss << ToString().GetData();

            if (addTypeName)
            {
                ss << ')';
            }
        }

        break;
    default:
        ss << ToString().GetData();
    }
}

HashCode Value::GetHashCode() const
{
    switch (m_type)
    {
    case Value::I8:
        return HashCode::GetHashCode(m_value.i8);
    case Value::I16:
        return HashCode::GetHashCode(m_value.i16);
    case Value::I32:
        return HashCode::GetHashCode(m_value.i32);
    case Value::I64:
        return HashCode::GetHashCode(m_value.i64);
    case Value::U8:
        return HashCode::GetHashCode(m_value.u8);
    case Value::U16:
        return HashCode::GetHashCode(m_value.u16);
    case Value::U32:
        return HashCode::GetHashCode(m_value.u32);
    case Value::U64:
        return HashCode::GetHashCode(m_value.u64);
    case Value::F32:
        return HashCode::GetHashCode(m_value.f);
    case Value::F64:
        return HashCode::GetHashCode(m_value.d);
    case Value::BOOLEAN:
        return HashCode::GetHashCode(m_value.b);
    case Value::VALUE_REF:
        Assert(m_value.internal.valueRef != nullptr);
        return m_value.internal.valueRef->GetHashCode();
    // case Value::HEAP_POINTER:
    //     if (m_value.internal.ptr == nullptr) {
    //         return HashCode::GetHashCode(0);
    //     } else if (VMString *string = m_value.internal.ptr->GetPointer<VMString>()) {
    //         return string->GetHashCode();
    //     } else if (VMArray *array = m_value.internal.ptr->GetPointer<VMArray>()) {
    //         return array->GetHashCode();
    //     } else if (VMArraySlice *slice = m_value.internal.ptr->GetPointer<VMArraySlice>()) {
    //         return slice->GetHashCode();
    //     } else if (VMObject *object = m_value.internal.ptr->GetPointer<VMObject>()) {
    //         return object->GetHashCode();
    //     } else {
    //         return HashCode::GetHashCode(m_value.internal.ptr);
    //     }
    case Value::USER_DATA:
        // hash the void*
        return HashCode::GetHashCode(m_value.internal.userData);
    case Value::HEAP_POINTER: // fallthrough
    default:
    {
        // If we get here, just stringify the value and hash that.
        const VMString str = ToString();

        return str.GetHashCode();
    }
    }
}

} // namespace vm
} // namespace hyperion
