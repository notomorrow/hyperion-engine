#pragma once

#include <script/vm/BytecodeStream.hpp>
#include <script/compiler/emit/InstructionStream.hpp>
#include <script/compiler/emit/Instruction.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>
#include <core/containers/String.hpp>
#include <util/UTF8.hpp>

#include <memory>

namespace hyperion::compiler {

class DecompilationUnit
{
public:
    DecompilationUnit();
    DecompilationUnit(const DecompilationUnit& other) = delete;

    void DecodeNext(
        uint8 code,
        hyperion::vm::BytecodeStream& bs,
        InstructionStream& is,
        std::ostream* os = nullptr);

    InstructionStream Decompile(
        hyperion::vm::BytecodeStream& bs,
        std::ostream* os = nullptr);
};

} // namespace hyperion::compiler

