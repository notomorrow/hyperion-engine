#ifndef BUILDABLE_HPP
#define BUILDABLE_HPP

#include <core/lib/DynArray.hpp>
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
    LabelPosition   position;
    Name            name;

    /**
     * Used in the case of 'break', 'continue' statements, to refer to the nearest label with the same name.
     */
    Bool            is_external = false;
};

struct BuildParams
{
    SizeType            block_offset = 0;
    SizeType            local_offset = 0;
    Array<LabelInfo>    labels;
};

struct Buildable
{
    virtual ~Buildable() = default;
};

} // namespace hyperion::compiler

#endif
