#ifndef HYPERION_VM_STRUCT_HPP
#define HYPERION_VM_STRUCT_HPP

#include <core/memory/ByteBuffer.hpp>
#include <core/utilities/Span.hpp>
#include <core/utilities/Variant.hpp>
#include <core/containers/LinkedList.hpp>
#include <script/vm/Value.hpp>

#include <core/Types.hpp>

namespace hyperion {
namespace vm {

enum VMStructType : uint8
{
    VM_STRUCT_TYPE_NONE = 0,
    VM_STRUCT_TYPE_I8,
    VM_STRUCT_TYPE_I16,
    VM_STRUCT_TYPE_I32,
    VM_STRUCT_TYPE_I64,
    VM_STRUCT_TYPE_U8,
    VM_STRUCT_TYPE_U16,
    VM_STRUCT_TYPE_U32,
    VM_STRUCT_TYPE_U64,
    VM_STRUCT_TYPE_F32,
    VM_STRUCT_TYPE_F64,

    VM_STRUCT_TYPE_DYNAMIC,

    VM_STRUCT_TYPE_STRING = VM_STRUCT_TYPE_DYNAMIC,
    VM_STRUCT_TYPE_STRUCT
};

using VMStructMemory        = ByteBuffer;
using VMStructMemoryView    = Span<const ubyte>;

struct VMStructMemberView
{
    uint32                                          offset; // offset in binary object

    VMStructType                                    type;
    VMStructMemoryView                              name_view;
    Variant<VMStructMemoryView, Value::ValueData>   data_view;
};

struct VMStructView
{
    Array<VMStructMemberView>   members;
};

struct VMStructDynamicMemory
{
    Array<Value>    values;
};

struct VMStructHeader
{
    uint32              count;
    uint32              total_size;
    uint32              *offsets;
    VMStructType        *types;
    char                **names;
};

struct VMStructDefinition
{
    Array<Pair<String, Value>> members;
};

struct VMStructMember
{
    VMStructType type;
    ANSIString name;
    Variant<ByteBuffer, Value::ValueData> data_buffer;
};

class VMStruct
{
public:
    static VMStruct MakeStruct(const VMStructDefinition &definition);
    static VMStructType ToStructType(vm::Value::ValueType value_type);
    static uint8 GetByteSize(VMStructType type);

    VMStruct() = default;

    VMStruct(const VMStruct &other) = delete;
    VMStruct &operator=(const VMStruct &other) = delete;

#if 0
    VMStruct(const VMStruct &other)
    {
        m_header.count = other.m_header.count;
        
        m_header.total_size = 0;
        m_header.offsets = new uint32[other.m_header.count];
        m_header.types = new VMStructType[other.m_header.count];
        m_header.names = new char*[other.m_header.count];
        m_header.dynamic = VMStructDynamicData {
            new Value[other.m_header.count]
        };

        for (uint32 index = 0; index < other.m_header.count; index++) {
            const SizeType length = std::strlen(other.m_header.names[index]);

            m_header.names[index] = new char[length + 1];
            std::strncpy(m_header.names[index], other.m_header.names[index], length);
        }

        m_bytes = other.m_bytes;
    }

    VMStruct &operator=(const VMStruct &other)
    {
        if (std::addressof(*this) == std::addressof(other)) {
            return *this;
        }

        {
            delete[] m_header.offsets;
            delete[] m_header.dynamic.values;
            delete[] m_header.types;

            for (uint32 index = 0; index < m_header.count; index++) {
                delete[] m_header.names[index];
            }

            delete[] m_header.names;
        }

        {
            m_header.count = other.m_header.count;
        
            m_header.total_size = 0;
            m_header.offsets = new uint32[other.m_header.count];
            m_header.types = new VMStructType[other.m_header.count];
            m_header.names = new char*[other.m_header.count];
            m_header.dynamic = VMStructDynamicData {
                new Value[other.m_header.count]
            };

            for (uint32 index = 0; index < other.m_header.count; index++) {
                const SizeType length = std::strlen(other.m_header.names[index]);

                m_header.names[index] = new char[length + 1];
                std::strncpy(m_header.names[index], other.m_header.names[index], length);
            }

            m_bytes = other.m_bytes;
        }

        return *this;
    }
#endif

    VMStruct(VMStruct &&other) noexcept
        : m_header(other.m_header),
          m_bytes(std::move(other.m_bytes))
    {
        other.m_header = { };
    }

    VMStruct &operator=(VMStruct &&other) noexcept
    {
        if (std::addressof(*this) == std::addressof(other)) {
            return *this;
        }

        other.m_header = m_header;
        other.m_bytes = std::move(m_bytes);

        m_header = VMStructHeader { };

        return *this;
    }

    ~VMStruct()
    {
        delete[] m_header.offsets;
        delete[] m_header.types;

        for (uint32 index = 0; index < m_header.count; index++) {
            delete[] m_header.names[index];
        }

        delete[] m_header.names;
    }

    const VMStructMemory &GetMemory() const
        { return m_bytes; }

    Array<Value> &GetDynamicMemberValues()
        { return m_dynamic_memory.values; }

    const Array<Value> &GetDynamicMemberValues() const
        { return m_dynamic_memory.values; }

    Value ReadMember(const char *name) const;
    bool WriteMember(const char *name, Value value);

private:
    VMStructHeader          m_header;
    VMStructDynamicMemory   m_dynamic_memory;
    VMStructMemory          m_bytes;
};

} // namespace vm
} // namespace hyperion

#endif