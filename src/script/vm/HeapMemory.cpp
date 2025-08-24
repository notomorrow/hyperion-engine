#include <script/vm/HeapMemory.hpp>

#include <script/vm/VMObject.hpp>
#include <script/vm/VMArray.hpp>
#include <script/vm/VMMemoryBuffer.hpp>
#include <script/vm/VMArraySlice.hpp>
#include <script/vm/Value.hpp>
#include <script/vm/VMString.hpp>
#include <script/vm/VMTypeInfo.hpp>

#include <iostream>
#include <iomanip>
#include <bitset>
#include <sstream>

namespace hyperion {
namespace vm {

std::ostream& operator<<(std::ostream& os, const Heap& heap)
{
    // print table header
    os << std::left;
    os << std::setw(16) << "Address" << "| ";
    os << std::setw(8) << "Flags" << "| ";
    os << std::setw(10) << "Type" << "| ";
    os << std::setw(16) << "Value";
    os << std::endl;

    HeapNode* tmpHead = heap.m_head;
    while (tmpHead != nullptr)
    {
        os << std::setw(16) << (void*)tmpHead << "| ";
        os << std::setw(8) << std::bitset<sizeof(tmpHead->value.GetFlags())>(tmpHead->value.GetFlags()) << "| ";
        os << std::setw(10);

        {
            union
            {
                VMString* strPtr;
                VMArray* arrayPtr;
                VMMemoryBuffer* memoryBufferPtr;
                VMArraySlice* slicePtr;
                VMObject* objPtr;
                VMTypeInfo* typeInfoPtr;
            } data;

            if (!tmpHead->value.HasValue())
            {
                os << "NullType" << "| ";

                os << std::setw(16);
                os << "null";
            }
            else if ((data.strPtr = tmpHead->value.GetPointer<VMString>()) != nullptr)
            {
                os << "String" << "| ";

                os << "\"" << data.strPtr->GetData() << "\"" << std::setw(16);
            }
            else if ((data.arrayPtr = tmpHead->value.GetPointer<VMArray>()) != nullptr)
            {
                os << "Array" << "| ";

                os << std::setw(16);

                std::stringstream ss;
                data.arrayPtr->GetRepresentation(ss, false);
                os << ss.rdbuf();
            }
            else if ((data.memoryBufferPtr = tmpHead->value.GetPointer<VMMemoryBuffer>()) != nullptr)
            {
                os << "MemoryBuffer" << "| ";

                os << std::setw(16);

                std::stringstream ss;
                data.memoryBufferPtr->GetRepresentation(ss, false);
                os << ss.rdbuf();
            }
            else if ((data.slicePtr = tmpHead->value.GetPointer<VMArraySlice>()) != nullptr)
            {
                os << "ArraySlice" << "| ";
                os << std::setw(16);

                std::stringstream ss;
                data.slicePtr->GetRepresentation(ss, false);
                os << ss.rdbuf();
            }
            else if ((data.objPtr = tmpHead->value.GetPointer<VMObject>()) != nullptr)
            {
                os << "Object" << "| ";

                os << std::setw(16);
                std::stringstream ss;
                data.objPtr->GetRepresentation(ss, false);
                os << ss.rdbuf();
            }
            else if ((data.typeInfoPtr = tmpHead->value.GetPointer<VMTypeInfo>()) != nullptr)
            {
                os << "TypeInfo" << "| ";

                os << std::setw(16);
                os << " ";
            }
            else
            {
                os << "Pointer" << "| ";

                os << std::setw(16);
                os << " ";
            }
        }

        os << std::endl;

        tmpHead = tmpHead->before;
    }

    return os;
}

Heap::Heap()
    : m_head(nullptr),
      m_numObjects(0)
{
}

Heap::~Heap()
{
    // purge the heap on destructor
    Purge();
}

void Heap::Purge()
{
    // clean up all allocated objects
    while (m_head)
    {
        HeapNode* tmp = m_head;
        m_head = tmp->before;
        delete tmp;

        m_numObjects--;
    }
}

HeapValue* Heap::Alloc()
{
    auto* node = new HeapNode;

    node->after = nullptr;

    if (m_head != nullptr)
    {
        m_head->after = node;
    }

    node->before = m_head;
    m_head = node;

    m_numObjects++;

    return &m_head->value;
}

void Heap::Sweep(uint32* outNumCollected)
{
    uint32 numCollected = 0;

    HeapNode* last = m_head;

    while (last)
    {
        if (last->value.GetFlags() & GC_ALIVE)
        {
            // the object is currently marked, so
            // we unmark it for the next time
            last->value.GetFlags() &= ~GC_MARKED;
            last = last->before;
        }
        else
        {
            // unmarked object, so delete it

            HeapNode* after = last->after;
            HeapNode* before = last->before;

            if (before)
            {
                before->after = after;
            }

            if (after)
            {
                // removing an item from the middle, so
                // make the nodes to the other sides now
                // point to each other
                after->before = before;
            }
            else
            {
                // since there are no nodes after this,
                // set the head to be this node here
                m_head = before;
            }

            Assert(!(last->value.GetFlags() & GC_DESTROYED));
            last->value.GetFlags() |= GC_DESTROYED;

            // delete the middle node
            delete last;
            last = before;

            // decrement number of currently allocated
            // objects
            --m_numObjects;
            ++numCollected;
        }
    }

    if (outNumCollected)
    {
        *outNumCollected = numCollected;
    }
}

} // namespace vm
} // namespace hyperion
