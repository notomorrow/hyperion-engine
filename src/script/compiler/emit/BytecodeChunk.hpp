#ifndef BYTECODE_CHUNK_HPP
#define BYTECODE_CHUNK_HPP

#include <script/compiler/emit/Instruction.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/Optional.hpp>
#include <core/Name.hpp>

#include <system/Debug.hpp>

#include <vector>
#include <memory>

namespace hyperion::compiler {

struct BytecodeChunk final : public Buildable
{
    Array<LabelInfo> labels;

    BytecodeChunk();
    BytecodeChunk(const BytecodeChunk &other) = delete;
    virtual ~BytecodeChunk() = default;

    void Append(std::unique_ptr<Buildable> buildable)
    {
        if (buildable != nullptr) {
            buildables.PushBack(std::move(buildable));
        }
    }

    LabelId NewLabel()
    {
        LabelId index = labels.Size();
        labels.PushBack(LabelInfo { UInt32(-1), HYP_NAME(Unnamed Label) });
        return index;
    }

    LabelId NewLabel(Name name, Bool is_external = false)
    {
        AssertThrowMsg(!FindLabelByName(name).HasValue(), "Cannot duplicate label identifier");

        LabelId index = labels.Size();
        labels.PushBack(LabelInfo { UInt32(-1), name, is_external });
        return index;
    }

    Optional<LabelId> FindLabelByName(Name name) const
    {
        const auto it = labels.FindIf([name](const LabelInfo &label_info) {
            return label_info.name == name;
        });

        if (it == labels.End()) {
            return Optional<LabelId> { };
        }

        return std::distance(labels.Begin(), it);
    }

    Array<std::unique_ptr<Buildable>> buildables;
};

} // namespace hyperion::compiler

#endif
