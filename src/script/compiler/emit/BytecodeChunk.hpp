#ifndef BYTECODE_CHUNK_HPP
#define BYTECODE_CHUNK_HPP

#include <script/compiler/emit/Instruction.hpp>

#include <core/containers/Array.hpp>

#include <core/utilities/Optional.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/debug/Debug.hpp>

#include <core/Name.hpp>

namespace hyperion::compiler {

struct BytecodeChunk final : public Buildable
{
    Array<LabelId> labels;

    BytecodeChunk();
    BytecodeChunk(const BytecodeChunk& other) = delete;
    virtual ~BytecodeChunk() = default;

    void Append(UniquePtr<Buildable>&& buildable)
    {
        if (buildable != nullptr)
        {
            buildables.PushBack(std::move(buildable));
        }
    }

    void TakeOwnershipOfLabel(LabelId labelId)
    {
        labels.PushBack(labelId);
    }

    Array<UniquePtr<Buildable>> buildables;
};

} // namespace hyperion::compiler

#endif
