#include <script/vm/StaticMemory.hpp>
#include <script/vm/HeapValue.hpp>

namespace hyperion {
namespace vm {

const uint16 StaticMemory::staticSize = 65535;

StaticMemory::StaticMemory()
    : m_data(new Value[staticSize])
{
    Memory::MemSet(m_data, 0, staticSize * sizeof(Value));
}

StaticMemory::~StaticMemory()
{
    // purge the items that are owned by this object
    MarkAllForDeallocation();
    // lastly delete the array
    delete[] m_data;
}

void StaticMemory::MarkAllForDeallocation()
{
    // delete all objects that are heap allocated
    for (uint32 i = staticSize; i != 0; i--)
    {
        Value& sv = m_data[i - 1];

        if (sv.m_type == Value::HEAP_POINTER && sv.m_value.internal.ptr != nullptr)
        {
            sv.m_value.internal.ptr->DisableFlags(GC_ALWAYS_ALIVE);
        }
    }
}

} // namespace vm
} // namespace hyperion
