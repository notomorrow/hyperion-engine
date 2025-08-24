#include <script/compiler/emit/codegen/InternalByteStream.hpp>

#include <core/debug/Debug.hpp>
#include <iostream>

namespace hyperion::compiler {

void InternalByteStream::MarkLabel(LabelId labelId)
{
    //m_labels[labelId] = LabelInfo {
    //    LabelPosition(m_stream.Size())
    //};
}

void InternalByteStream::AddFixup(LabelId labelId, SizeType position, SizeType offset)
{
    Fixup fixup;
    fixup.labelId = labelId;
    fixup.position = position;
    fixup.offset   = offset;

    m_fixups.PushBack(fixup);

    Assert(m_stream.Size() >= position + sizeof(LabelPosition), "Not enough space allotted for the LabelPosition");

    // advance position by adding placeholder bytes
    for (SizeType i = 0; i < sizeof(LabelPosition); i++) {
        m_stream[position + i] = ubyte(-1);
    }
}

void InternalByteStream::AddFixup(LabelId labelId, SizeType offset)
{
    const SizeType position = m_stream.Size();

    m_stream.Resize(m_stream.Size() + sizeof(LabelPosition));

    AddFixup(labelId, position, offset);
}

void InternalByteStream::Bake(const BuildParams &buildParams)
{

    for (const Fixup &fixup : m_fixups) {
        auto it = buildParams.labels.FindIf([labelId = fixup.labelId](const LabelInfo &labelInfo)
        {
            return labelInfo.labelId == labelId;
        });

        Assert(it != buildParams.labels.End(), "No label with fixup ID was found");

        LabelPosition labelPosition = it->position;
        Assert(labelPosition != LabelPosition(-1), "Label position not set!");
        //labelPosition += fixup.offset;

        const SizeType fixupPosition = fixup.position;
        
        // first, make sure there is enough space in the stream at that position
        Assert(m_stream.Size() >= fixupPosition + sizeof(LabelPosition), "Not enough space allotted for the LabelPosition");
        
        // now, make sure that each item in the buffer has been set to -1
        // if this is not the case, it is likely the wrong position
        for (SizeType i = 0; i < sizeof(LabelPosition); i++) {
            Assert(m_stream[fixupPosition + i] == ubyte(-1), "Placeholder value in buffer should be set to -1");
            m_stream[fixupPosition + i] = (labelPosition >> (i << 3)) & 0xFF;
        }
    }

    // clear fixups vector, no more work to do
    m_fixups.Clear();
}

} // namespace hyperion::compiler
