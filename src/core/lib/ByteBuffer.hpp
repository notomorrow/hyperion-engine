#ifndef HYPERION_V2_LIB_BYTE_BUFFER_HPP
#define HYPERION_V2_LIB_BYTE_BUFFER_HPP

#include <core/lib/RefCountedPtr.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/Variant.hpp>
#include <HashCode.hpp>

#include <Types.hpp>

namespace hyperion {

/*! \brief An immutable array of bytes, which for large buffers, shares the memory with any copied objects */
class ByteBuffer
{
    using InternalArray = DynArray<UByte, 1024u>;

public:
    ByteBuffer()
    {
        // default it to internal byte array
        m_internal.Set<InternalArray>({ });
    }

    ByteBuffer(SizeType count)
    {
        // default it to internal byte array
        m_internal.Set<InternalArray>({ });
        SetSize(count);
    }

    ByteBuffer(SizeType count, const void *data)
    {
        // default it to internal byte array
        m_internal.Set<InternalArray>({ });
        SetData(count, data);
    }

    ByteBuffer(const ByteBuffer &other)
        : m_internal(other.m_internal)
    {
    }

    ByteBuffer &operator=(const ByteBuffer &other)
    {
        m_internal = other.m_internal;

        return *this;
    }

    ByteBuffer(ByteBuffer &&other) noexcept
        : m_internal(std::move(other.m_internal))
    {
    }

    ByteBuffer &operator=(ByteBuffer &&other) noexcept
    {
        m_internal = std::move(other.m_internal);

        return *this;
    }

    ~ByteBuffer() = default;

    void SetData(SizeType count, const void *data)
    {
        if (count > InternalArray::num_inline_elements) {
            m_internal.Set(RefCountedPtr<InternalArray>(new InternalArray()));

            auto &byte_array = m_internal.Get<RefCountedPtr<InternalArray>>();

            byte_array->Resize(count);
            Memory::Copy(byte_array->Data(), data, count);
        } else {
            m_internal.Set(InternalArray { });

            auto &byte_array = m_internal.Get<InternalArray>();

            byte_array.Resize(count);
            Memory::Copy(byte_array.Data(), data, count);
        }
    }

    InternalArray &GetInternalArray()
    {
        if (m_internal.Is<RefCountedPtr<InternalArray>>()) {
            return *m_internal.Get<RefCountedPtr<InternalArray>>();
        } else {
            return m_internal.Get<InternalArray>();
        }
    }

    const InternalArray &GetInternalArray() const
        { return const_cast<ByteBuffer *>(this)->GetInternalArray(); }

    const UByte *Data() const
        { return GetInternalArray().Data(); }

    SizeType Size() const
        { return GetInternalArray().Size(); }

    bool Any() const
        { return Size() != 0; }

    bool Empty() const
        { return Size() == 0; }

    const UByte &operator[](SizeType index) const
        { return GetInternalArray()[index]; }

    bool operator==(const ByteBuffer &other) const
        { return GetInternalArray() == other.GetInternalArray(); }

    bool operator!=(const ByteBuffer &other) const
        { return GetInternalArray() != other.GetInternalArray(); }

    HashCode GetHashCode() const
        { return GetInternalArray().GetHashCode(); }

private:
    Variant<InternalArray, RefCountedPtr<InternalArray>> m_internal;

    void SetSize(SizeType count)
    {
        if (count > InternalArray::num_inline_elements) {
            m_internal.Set(RefCountedPtr<InternalArray>(new InternalArray()));

            auto &byte_array = m_internal.Get<RefCountedPtr<InternalArray>>();

            byte_array->Resize(count);
        } else {
            m_internal.Set(InternalArray { });

            auto &byte_array = m_internal.Get<InternalArray>();

            byte_array.Resize(count);
        }
    }
};

} // namespace hyperion

#endif