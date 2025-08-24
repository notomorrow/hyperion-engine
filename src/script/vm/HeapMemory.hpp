#ifndef HEAP_MEMORY_HPP
#define HEAP_MEMORY_HPP

#include <script/vm/HeapValue.hpp>
#include <core/Types.hpp>

#include <ostream>

namespace hyperion {
namespace vm {

struct HeapNode
{
    HeapValue   value;
    HeapNode    *before;
    HeapNode    *after;
};

class Heap
{
    friend std::ostream &operator<<(std::ostream &os, const Heap &heap);
public:
    Heap();

    Heap(const Heap &other)             = delete;
    Heap &operator=(const Heap &other)  = delete;
    
    Heap(Heap &&other)                  = delete;
    Heap &operator=(Heap &&other)       = delete;

    ~Heap();

    SizeType Size() const
        { return m_numObjects; }

    /** Destroy everything on the heap */
    void Purge();
    /** Allocate a new value on the heap. */
    HeapValue *Alloc();
    /** Delete all nodes that are not marked */
    void Sweep(uint32 *outNumCollected = nullptr);

private:
    HeapNode *m_head;
    SizeType m_numObjects;
};

} // namespace vm
} // namespace hyperion

#endif
