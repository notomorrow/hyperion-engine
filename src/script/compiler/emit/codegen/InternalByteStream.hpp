#ifndef INTERNAL_BYTE_STREAM_HPP
#define INTERNAL_BYTE_STREAM_HPP

#include <script/compiler/emit/Buildable.hpp>
#include <core/Types.hpp>

#include <map>
#include <sstream>
#include <vector>
#include <cstdint>

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
        return m_stream.Size();
    }

    const Array<ubyte>& GetData() const
    {
        return m_stream;
    }

    const Array<Fixup>& GetFixups() const
    {
        return m_fixups;
    }

    void Put(ubyte byte)
    {
        m_stream.PushBack(byte);
    }

    void Put(const ubyte* bytes, SizeType size)
    {
        for (SizeType i = 0; i < size; i++)
        {
            m_stream.PushBack(bytes[i]);
        }
    }

    void MarkLabel(LabelId labelId);
    void AddFixup(LabelId labelId, SizeType position, SizeType offset);
    void AddFixup(LabelId labelId, SizeType offset);

    void Bake(const BuildParams& buildParams);

private:
    Array<ubyte> m_stream;
    Array<Fixup> m_fixups;
};

} // namespace hyperion::compiler

#endif
