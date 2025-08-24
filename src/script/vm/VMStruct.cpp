#include <script/vm/VMStruct.hpp>

#include <core/debug/Debug.hpp>

#include <cmath>
#include <cstring>
#include <sstream>

namespace hyperion {
namespace vm {

VMStruct VMStruct::MakeStruct(const VMStructDefinition& definition)
{
    Assert(definition.members.Size() <= UINT32_MAX);

    VMStruct result;

    result.m_header.count = uint32(definition.members.Size());
    result.m_header.totalSize = 0;
    result.m_header.offsets = new uint32[definition.members.Size()];
    result.m_header.types = new VMStructType[definition.members.Size()];
    result.m_header.names = new char*[definition.members.Size()];

    {
        SizeType offset = 0;

        for (SizeType index = 0; index < definition.members.Size(); index++)
        {
            result.m_header.offsets[index] = uint32(offset);

            const VMStructType type = ToStructType(definition.members[index].second.GetType());

            result.m_header.types[index] = type;
            result.m_header.names[index] = new char[definition.members[index].first.Size() + 1];

            Memory::StrCpy(
                result.m_header.names[index],
                definition.members[index].first.Data(),
                definition.members[index].first.Size() + 1);

            const uint8 byteSize = type < VM_STRUCT_TYPE_DYNAMIC
                ? GetByteSize(type)
                : uint8(sizeof(void*)) /* Sizeof pointer */;

            offset += byteSize;
        }

        result.m_header.totalSize = uint32(offset);
    }

    result.m_bytes.SetSize(result.m_header.totalSize);

    result.m_dynamicMemory.values.Resize(result.m_header.count);

    for (uint32 index = 0; index < result.m_header.count; index++)
    {
        VMStructType type = result.m_header.types[index];
        uint32 offset = result.m_header.offsets[index];

        const uint32 memberSize = index != result.m_header.count - 1
            ? result.m_header.offsets[index + 1] - offset
            : result.m_header.totalSize - offset;

        const Value& value = definition.members[index].second;

        if (type < VM_STRUCT_TYPE_DYNAMIC)
        {
            Assert(memberSize <= sizeof(Value::ValueData), "Byte size of struct member must be <= sizeof(Value::ValueData)");

            const Value::ValueData& valueData = value.m_value;

            // read next n bytes
            switch (type)
            {
            case VM_STRUCT_TYPE_I8:
                result.m_bytes.Write(memberSize, offset, &valueData.i8);
                break;
            case VM_STRUCT_TYPE_U8:
                result.m_bytes.Write(memberSize, offset, &valueData.u8);
                break;
            case VM_STRUCT_TYPE_I16:
                result.m_bytes.Write(memberSize, offset, &valueData.i16);
                break;
            case VM_STRUCT_TYPE_U16:
                result.m_bytes.Write(memberSize, offset, &valueData.u16);
                break;
            case VM_STRUCT_TYPE_I32:
                result.m_bytes.Write(memberSize, offset, &valueData.i32);
                break;
            case VM_STRUCT_TYPE_U32:
                result.m_bytes.Write(memberSize, offset, &valueData.u32);
                break;
            case VM_STRUCT_TYPE_I64:
                result.m_bytes.Write(memberSize, offset, &valueData.i64);
                break;
            case VM_STRUCT_TYPE_U64:
                result.m_bytes.Write(memberSize, offset, &valueData.u64);
                break;
            case VM_STRUCT_TYPE_F32:
                result.m_bytes.Write(memberSize, offset, &valueData.f);
                break;
            case VM_STRUCT_TYPE_F64:
                result.m_bytes.Write(memberSize, offset, &valueData.d);
                break;
            default:
                result.m_bytes.Write(memberSize, offset, &valueData.i8);
                break;
            }
        }
        else
        {
            Assert(value.m_type == Value::ValueType::HEAP_POINTER);
            Assert(memberSize == sizeof(void*));

            void* vp = GetRawPointerForHeapValue(value.m_value.ptr);
            result.m_bytes.Write(sizeof(void*), offset, &vp);

            result.m_dynamicMemory.values[index] = value;
        }
    }

    return result;
}

VMStructType VMStruct::ToStructType(vm::Value::ValueType valueType)
{
    switch (valueType)
    {
    case Value::ValueType::I8:
        return VM_STRUCT_TYPE_I8;
    case Value::ValueType::U8:
        return VM_STRUCT_TYPE_U8;
    case Value::ValueType::I16:
        return VM_STRUCT_TYPE_I16;
    case Value::ValueType::U16:
        return VM_STRUCT_TYPE_U16;
    case Value::ValueType::I32:
        return VM_STRUCT_TYPE_I32;
    case Value::ValueType::U32:
        return VM_STRUCT_TYPE_U32;
    case Value::ValueType::I64:
        return VM_STRUCT_TYPE_I64;
    case Value::ValueType::U64:
        return VM_STRUCT_TYPE_U64;
    case Value::ValueType::F32:
        return VM_STRUCT_TYPE_F32;
    case Value::ValueType::F64:
        return VM_STRUCT_TYPE_F64;
    case Value::ValueType::BOOLEAN:
        return VM_STRUCT_TYPE_U8;
    case Value::ValueType::HEAP_POINTER:
        return VM_STRUCT_TYPE_DYNAMIC;
    default:
        return VM_STRUCT_TYPE_NONE;
    }
}

uint8 VMStruct::GetByteSize(VMStructType type)
{
    if (type >= VM_STRUCT_TYPE_DYNAMIC)
    {
        return uint8(sizeof(void*));
    }

    switch (type)
    {
    case VM_STRUCT_TYPE_I8:
        return 1;
    case VM_STRUCT_TYPE_U8:
        return 1;
    case VM_STRUCT_TYPE_I16:
        return 2;
    case VM_STRUCT_TYPE_U16:
        return 2;
    case VM_STRUCT_TYPE_I32:
        return 4;
    case VM_STRUCT_TYPE_U32:
        return 4;
    case VM_STRUCT_TYPE_I64:
        return 8;
    case VM_STRUCT_TYPE_U64:
        return 8;
    case VM_STRUCT_TYPE_F32:
        return 4;
    case VM_STRUCT_TYPE_F64:
        return 4;
    default:
        return uint8(-1);
    }
}

Value VMStruct::ReadMember(const char* name) const
{
    Assert(m_header.names != nullptr, "Struct not initialized");

    for (uint32 index = 0; index < m_header.count; index++)
    {
        if (std::strcmp(name, m_header.names[index]) == 0)
        {
            if (m_header.types[index] < VM_STRUCT_TYPE_DYNAMIC)
            {
                Value::ValueData data;

                switch (m_header.types[index])
                {
                case VM_STRUCT_TYPE_I8:
                    m_bytes.Read(m_header.offsets[index], 1, reinterpret_cast<ubyte*>(&data.i8));

                    return Value(Value::ValueType::I8, data);
                case VM_STRUCT_TYPE_U8:
                    m_bytes.Read(m_header.offsets[index], 1, reinterpret_cast<ubyte*>(&data.u8));

                    return Value(Value::ValueType::U8, data);
                case VM_STRUCT_TYPE_I16:
                    m_bytes.Read(m_header.offsets[index], 2, reinterpret_cast<ubyte*>(&data.i16));

                    return Value(Value::ValueType::I16, data);
                case VM_STRUCT_TYPE_U16:
                    m_bytes.Read(m_header.offsets[index], 2, reinterpret_cast<ubyte*>(&data.u16));

                    return Value(Value::ValueType::U16, data);
                case VM_STRUCT_TYPE_I32:
                    m_bytes.Read(m_header.offsets[index], 4, reinterpret_cast<ubyte*>(&data.i32));

                    return Value(Value::ValueType::I32, data);
                case VM_STRUCT_TYPE_U32:
                    m_bytes.Read(m_header.offsets[index], 4, reinterpret_cast<ubyte*>(&data.u32));

                    return Value(Value::ValueType::U32, data);
                case VM_STRUCT_TYPE_I64:
                    m_bytes.Read(m_header.offsets[index], 8, reinterpret_cast<ubyte*>(&data.i64));

                    return Value(Value::ValueType::I64, data);
                case VM_STRUCT_TYPE_U64:
                    m_bytes.Read(m_header.offsets[index], 8, reinterpret_cast<ubyte*>(&data.u64));

                    return Value(Value::ValueType::U64, data);
                case VM_STRUCT_TYPE_F32:
                    m_bytes.Read(m_header.offsets[index], 4, reinterpret_cast<ubyte*>(&data.f));

                    return Value(Value::ValueType::F32, data);
                case VM_STRUCT_TYPE_F64:
                    m_bytes.Read(m_header.offsets[index], 8, reinterpret_cast<ubyte*>(&data.d));

                    return Value(Value::ValueType::F64, data);
                default:
                    Assert(false, "Not implemented");
                }
            }
            else
            {
                return m_dynamicMemory.values[index];
            }
        }
    }

    return Value(Value::ValueType::HEAP_POINTER, { .ptr = nullptr });
}

bool VMStruct::WriteMember(const char* name, Value value)
{
    Assert(m_header.names != nullptr, "Struct not allocated");

    uint32 memberIndex = uint32(-1);

    for (uint32 index = 0; index < m_header.count; index++)
    {
        if (std::strcmp(name, m_header.names[index]) == 0)
        {
            memberIndex = index;

            break;
        }
    }

    if (memberIndex == uint32(-1))
    {
        return false;
    }

    const uint32 offset = m_header.offsets[memberIndex];
    const VMStructType type = m_header.types[memberIndex];
    const uint8 byteSize = GetByteSize(type);

    Assert(offset + byteSize <= m_header.totalSize);

    const VMStructType givenType = ToStructType(value.GetType());

    Value::ValueData newValueData;

    switch (givenType)
    {
    case VM_STRUCT_TYPE_I8: // fallthrough
    case VM_STRUCT_TYPE_I16:
    case VM_STRUCT_TYPE_I32:
    case VM_STRUCT_TYPE_I64:
        if (!value.GetInteger(&newValueData.i64))
        {
            return false;
        }

        break;
    case VM_STRUCT_TYPE_U8: // fallthrough
    case VM_STRUCT_TYPE_U16:
    case VM_STRUCT_TYPE_U32:
    case VM_STRUCT_TYPE_U64:
        if (!value.GetUnsigned(&newValueData.u64))
        {
            return false;
        }

        break;
    case VM_STRUCT_TYPE_F32: // fallthrough
    case VM_STRUCT_TYPE_F64:
        if (!value.GetFloatingPointCoerce(&newValueData.d))
        {
            return false;
        }

        break;
    default:
        if (givenType >= VM_STRUCT_TYPE_DYNAMIC)
        {
            if (!value.GetPointer(&newValueData.ptr))
            {
                return false;
            }
        }

        return false;
    }

    switch (type)
    {
    case VM_STRUCT_TYPE_I8:
        Assert(byteSize == sizeof(int8));

        newValueData.i8 = static_cast<int8>(newValueData.i64);

        m_bytes.Write(byteSize, offset, &newValueData.i8);

        break;
    case VM_STRUCT_TYPE_I16:
        Assert(byteSize == sizeof(int16));

        newValueData.i16 = static_cast<int16>(newValueData.i64);

        m_bytes.Write(byteSize, offset, &newValueData.i16);

        break;
    case VM_STRUCT_TYPE_I32:
        Assert(byteSize == sizeof(int32));

        newValueData.i32 = static_cast<int32>(newValueData.i64);

        m_bytes.Write(byteSize, offset, &newValueData.i32);

        break;
    case VM_STRUCT_TYPE_I64:
        Assert(byteSize == sizeof(int64));

        newValueData.i64 = static_cast<int64>(newValueData.i64);

        m_bytes.Write(byteSize, offset, &newValueData.i64);

        break;
    case VM_STRUCT_TYPE_U8:
        Assert(byteSize == sizeof(uint8));

        newValueData.u8 = static_cast<uint8>(newValueData.u64);

        m_bytes.Write(byteSize, offset, &newValueData.u8);

        break;
    case VM_STRUCT_TYPE_U16:
        Assert(byteSize == sizeof(uint16));

        newValueData.u16 = static_cast<uint16>(newValueData.u64);

        m_bytes.Write(byteSize, offset, &newValueData.u16);

        break;
    case VM_STRUCT_TYPE_U32:
        Assert(byteSize == sizeof(uint32));

        newValueData.u32 = static_cast<uint32>(newValueData.u64);

        m_bytes.Write(byteSize, offset, &newValueData.u32);

        break;
    case VM_STRUCT_TYPE_U64:
        Assert(byteSize == sizeof(uint64));

        newValueData.u64 = static_cast<uint64>(newValueData.u64);

        m_bytes.Write(byteSize, offset, &newValueData.u64);

        break;
    case VM_STRUCT_TYPE_F32:
        Assert(byteSize == sizeof(float));

        newValueData.f = static_cast<float>(newValueData.d);

        m_bytes.Write(byteSize, offset, &newValueData.f);

        break;
    case VM_STRUCT_TYPE_F64:
        Assert(byteSize == sizeof(double));

        newValueData.d = static_cast<double>(newValueData.d);

        m_bytes.Write(byteSize, offset, &newValueData.d);

        break;
    default:
        if (type >= VM_STRUCT_TYPE_DYNAMIC)
        {
            Assert(byteSize == sizeof(void*));

            m_dynamicMemory.values[memberIndex] = value;

            m_bytes.Write(byteSize, offset, &newValueData.ptr);

            break;
        }

        return false;
    }

    return true;
}

} // namespace vm
} // namespace hyperion
