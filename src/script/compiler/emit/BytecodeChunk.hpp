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
    Array<LabelId> labels;

    BytecodeChunk();
    BytecodeChunk(const BytecodeChunk &other) = delete;
    virtual ~BytecodeChunk() = default;

    void Append(std::unique_ptr<Buildable> buildable)
    {
        if (buildable != nullptr) {
            buildables.PushBack(std::move(buildable));
        }
    }

    void TakeOwnershipOfLabel(LabelId label_id)
    {
        labels.PushBack(label_id);
    }

    Array<std::unique_ptr<Buildable>> buildables;
};

} // namespace hyperion::compiler

#endif
