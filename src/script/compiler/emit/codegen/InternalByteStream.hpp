#ifndef INTERNAL_BYTE_STREAM_HPP
#define INTERNAL_BYTE_STREAM_HPP

#include <script/compiler/emit/Buildable.hpp>
#include <Types.hpp>

#include <map>
#include <sstream>
#include <vector>
#include <cstdint>

namespace hyperion::compiler {

struct Fixup
{
    LabelId     label_id = LabelId(-1);
    SizeType    position = SizeType(-1);
    SizeType    offset = SizeType(-1);
};

class InternalByteStream
{
public:
    SizeType GetPosition() const
        { return m_stream.Size(); }

    const Array<UByte> &GetData() const
        { return m_stream; }

    const Array<Fixup> &GetFixups() const
        { return m_fixups; }

    void Put(UByte byte)
        { m_stream.PushBack(byte); }

    void Put(const UByte *bytes, SizeType size)
    {
        for (SizeType i = 0; i < size; i++) {
            m_stream.PushBack(bytes[i]);
        }
    }

    void MarkLabel(LabelId label_id);
    void AddFixup(LabelId label_id, SizeType position, SizeType offset);
    void AddFixup(LabelId label_id, SizeType offset);

    void Bake(const BuildParams &build_params);

private:
    Array<UByte> m_stream;
    Array<Fixup> m_fixups;
};

} // namespace hyperion::compiler

#endif
