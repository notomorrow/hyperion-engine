#pragma once

#include <script/vm/Value.hpp>
#include <core/debug/Debug.hpp>
#include <core/Types.hpp>

#include <utility>

namespace hyperion {
namespace vm {

class StaticMemory
{
public:
    static const uint16 staticSize;

public:
    StaticMemory();
    StaticMemory(const StaticMemory& other) = delete;
    ~StaticMemory();

    /*! \brief Marks all values for deallocation,
        allowing the garbage collector to free them. */
    void MarkAllForDeallocation();

    HYP_FORCE_INLINE Value& operator[](SizeType index)
    {
        Assert(index < staticSize, "out of bounds");
        return m_data[index];
    }

    HYP_FORCE_INLINE const Value& operator[](SizeType index) const
    {
        Assert(index < staticSize, "out of bounds");
        return m_data[index];
    }

private:
    Value* m_data;
};

} // namespace vm
} // namespace hyperion

