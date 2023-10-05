#ifndef BUILDABLE_HPP
#define BUILDABLE_HPP

#include <core/lib/DynArray.hpp>
#include <core/lib/SortedArray.hpp>
#include <core/Name.hpp>
#include <Types.hpp>

namespace hyperion::compiler {

using byte = UByte;
using LabelPosition = UInt32;

using Opcode = UInt8;
using RegIndex = UInt8;
using LabelId = SizeType;

struct LabelInfo
{
    LabelId         label_id = LabelId(-1);
    LabelPosition   position = LabelPosition(-1);
    Name            name     = HYP_NAME(LabelNameNotSet);

    Bool operator==(const LabelInfo &other) const
    {
        return label_id == other.label_id
            && position == other.position
            && name == other.name;
    }

    Bool operator<(const LabelInfo &other) const
        { return label_id < other.label_id; }
};

struct BuildParams
{
    SizeType                block_offset = 0;
    SizeType                local_offset = 0;
    SortedArray<LabelInfo>  labels;
};

struct Buildable
{
    virtual ~Buildable() = default;
};

} // namespace hyperion::compiler

#endif
