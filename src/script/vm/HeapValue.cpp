#include <script/vm/HeapValue.hpp>
#include <script/vm/VMObject.hpp>
#include <script/vm/VMArray.hpp>
#include <script/vm/VMArraySlice.hpp>
#include <script/vm/VMString.hpp>

#include <iostream>

namespace hyperion {
namespace vm {

HeapValue::HeapValue()
    : m_flags(0)
{
}

HeapValue::~HeapValue() = default;

void HeapValue::Mark()
{
    AssertThrow(!(m_flags & GC_DESTROYED));

    m_flags |= GC_MARKED;

    if (VMObject *object = GetPointer<VMObject>()) {
        HeapValue *proto = nullptr;

        do {
            const auto size = object->GetSize();

            for (SizeType i = 0; i < size; i++) {
                object->GetMember(i).value.Mark();
            }

            if ((proto = object->GetPrototype()) != nullptr) {
                proto->Mark();

                object = proto->GetPointer<VMObject>();

                // get object value of prototype
                // if ((object = proto->GetPointer<VMObject>()) == nullptr) {
                //     // if prototype is not an object (has been modified to another value), we're done.
                //     return;
                // }
            } else {
                object = nullptr;
            }
        } while (object != nullptr);
    } else if (VMArray *array = GetPointer<VMArray>()) {
        const auto size = array->GetSize();

        for (SizeType i = 0; i < size; i++) {
            array->AtIndex(i).Mark();
        }
    } else if (VMArraySlice *slice = GetPointer<VMArraySlice>()) {
        const auto size = slice->GetSize();

        for (SizeType i = 0; i < size; i++) {
            slice->AtIndex(i).Mark();
        }
    }
}

} // namespace vm
} // namespace hyperion
