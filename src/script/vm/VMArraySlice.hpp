#pragma once

#include <script/vm/VMArray.hpp>
#include <script/vm/Value.hpp>
#include <core/HashCode.hpp>

namespace hyperion {
namespace vm {

class VMArraySlice
{
public:
    using SizeType = VMArray::SizeType;

    VMArraySlice(VMArray* ary, SizeType start, SizeType end);
    VMArraySlice(const VMArraySlice& other);

    VMArraySlice& operator=(const VMArraySlice& other);

    bool operator==(const VMArraySlice& other) const
    {
        return m_ary == other.m_ary
            && m_start == other.m_start
            && m_end == other.m_end;
    }

    SizeType GetSize() const
    {
        return m_end - m_start;
    }

    Value& AtIndex(SizeType index)
    {
        return m_ary->AtIndex(m_start + index);
    }
    const Value& AtIndex(SizeType index) const
    {
        return m_ary->AtIndex(m_start + index);
    }

    void GetRepresentation(
        std::stringstream& ss,
        bool addTypeName = true,
        int depth = 3) const;

    HashCode GetHashCode() const;

private:
    VMArray* m_ary;
    SizeType m_start;
    SizeType m_end;
};

} // namespace vm
} // namespace hyperion

