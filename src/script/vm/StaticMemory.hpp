#ifndef STATIC_MEMORY_HPP
#define STATIC_MEMORY_HPP

#include <script/vm/Value.hpp>
#include <system/Debug.hpp>
#include <Types.hpp>

#include <utility>

namespace hyperion {
namespace vm {

class StaticMemory
{
public:
    static const UInt16 static_size;

public:
    StaticMemory();
    StaticMemory(const StaticMemory &other) = delete;
    ~StaticMemory();

    /** Delete everything in static memory */
    void Purge();
    
    HYP_FORCE_INLINE
    Value &operator[](SizeType index)
    {
        AssertThrowMsg(index < static_size, "out of bounds");
        return m_data[index];
    }
    
    HYP_FORCE_INLINE
    const Value &operator[](SizeType index) const
    {
        AssertThrowMsg(index < static_size, "out of bounds");
        return m_data[index];
    }

    // move a value to static memory
    HYP_FORCE_INLINE
    void Store(Value value)
    {
        AssertThrowMsg(m_sp < static_size, "not enough static memory");
        m_data[m_sp++] = value;
    }

private:
    Value *m_data;
    SizeType m_sp;
};

} // namespace vm
} // namespace hyperion

#endif
