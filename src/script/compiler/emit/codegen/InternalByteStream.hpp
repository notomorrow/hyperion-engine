#pragma once

#include <script/compiler/emit/Buildable.hpp>

#include <core/memory/ByteBuffer.hpp>

#include <core/Types.hpp>

namespace hyperion::compiler {

struct Fixup
{
    LabelId labelId = LabelId(-1);
    SizeType position = SizeType(-1);
    SizeType offset = SizeType(-1);
};

class InternalByteStream
{
public:
    SizeType GetPosition() const
    {
        return m_byteBuffer.Size();
    }

    const ByteBuffer& GetData() const
    {
        return m_byteBuffer;
    }

    const Array<Fixup>& GetFixups() const
    {
        return m_fixups;
    }

    void Put(ubyte byte)
    {
        if (m_byteBuffer.Size() + 1 >= m_byteBuffer.GetCapacity())
        {
            m_byteBuffer.SetCapacity(m_byteBuffer.GetCapacity() * 2);
        }

        m_byteBuffer.SetSize(m_byteBuffer.Size() + 1);
        m_byteBuffer.Data()[m_byteBuffer.Size() - 1] = byte;
    }

    void Put(const ubyte* bytes, SizeType size)
    {
        if (m_byteBuffer.Size() + size >= m_byteBuffer.GetCapacity())
        {
            SizeType newCapacity = m_byteBuffer.GetCapacity() * 2;

            while (newCapacity < m_byteBuffer.Size() + size)
            {
                newCapacity *= 2;
            }

            m_byteBuffer.SetCapacity(newCapacity);
        }

        const SizeType oldSize = m_byteBuffer.Size();
        m_byteBuffer.SetSize(oldSize + size);

        for (SizeType i = 0; i < size; i++)
        {
            m_byteBuffer.Data()[oldSize + i] = bytes[i];
        }
    }

    void MarkLabel(LabelId labelId);
    void AddFixup(LabelId labelId, SizeType position, SizeType offset);
    void AddFixup(LabelId labelId, SizeType offset);

    void Bake(const BuildParams& buildParams);

private:
    ByteBuffer m_byteBuffer;
    Array<Fixup> m_fixups;
};

} // namespace hyperion::compiler
