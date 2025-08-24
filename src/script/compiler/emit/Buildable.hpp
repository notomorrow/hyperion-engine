#ifndef BUILDABLE_HPP
#define BUILDABLE_HPP

#include <core/containers/Array.hpp>
#include <core/containers/SortedArray.hpp>
#include <core/Name.hpp>
#include <core/Types.hpp>

namespace hyperion::compiler {

using byte = ubyte;
using LabelPosition = uint32;

using Opcode = uint8;
using RegIndex = uint8;
using LabelId = SizeType;

struct LabelInfo
{
    LabelId labelId = LabelId(-1);
    LabelPosition position = LabelPosition(-1);
    Name name = HYP_NAME(LabelNameNotSet);

    bool operator==(const LabelInfo& other) const
    {
        return labelId == other.labelId
            && position == other.position
            && name == other.name;
    }

    bool operator<(const LabelInfo& other) const
    {
        return labelId < other.labelId;
    }
};

struct BuildParams
{
    SizeType blockOffset = 0;
    SizeType localOffset = 0;
    SortedArray<LabelInfo> labels;
};

struct Buildable
{
    virtual ~Buildable() = default;
};

} // namespace hyperion::compiler

#endif
